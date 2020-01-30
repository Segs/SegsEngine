/*************************************************************************/
/*  visual_shader_nodes.cpp                                              */
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

#include "visual_shader_nodes.h"

#include "core/method_bind.h"
#include "core/string_formatter.h"
#include "core/translation_helpers.h"

IMPL_GDCLASS(VisualShaderNodeScalarConstant)
IMPL_GDCLASS(VisualShaderNodeBooleanConstant)
IMPL_GDCLASS(VisualShaderNodeColorConstant)
IMPL_GDCLASS(VisualShaderNodeVec3Constant)
IMPL_GDCLASS(VisualShaderNodeTransformConstant)
IMPL_GDCLASS(VisualShaderNodeTexture)
IMPL_GDCLASS(VisualShaderNodeCubeMap)
IMPL_GDCLASS(VisualShaderNodeScalarOp)
IMPL_GDCLASS(VisualShaderNodeVectorOp)
IMPL_GDCLASS(VisualShaderNodeColorOp)
IMPL_GDCLASS(VisualShaderNodeTransformMult)
IMPL_GDCLASS(VisualShaderNodeTransformVecMult)
IMPL_GDCLASS(VisualShaderNodeScalarFunc)
IMPL_GDCLASS(VisualShaderNodeVectorFunc)
IMPL_GDCLASS(VisualShaderNodeColorFunc)
IMPL_GDCLASS(VisualShaderNodeTransformFunc)
IMPL_GDCLASS(VisualShaderNodeDotProduct)
IMPL_GDCLASS(VisualShaderNodeVectorLen)
IMPL_GDCLASS(VisualShaderNodeDeterminant)
IMPL_GDCLASS(VisualShaderNodeScalarClamp)
IMPL_GDCLASS(VisualShaderNodeVectorClamp)
IMPL_GDCLASS(VisualShaderNodeScalarDerivativeFunc)
IMPL_GDCLASS(VisualShaderNodeVectorDerivativeFunc)
IMPL_GDCLASS(VisualShaderNodeFaceForward)
IMPL_GDCLASS(VisualShaderNodeOuterProduct)
IMPL_GDCLASS(VisualShaderNodeVectorScalarStep)
IMPL_GDCLASS(VisualShaderNodeScalarSmoothStep)
IMPL_GDCLASS(VisualShaderNodeVectorSmoothStep)
IMPL_GDCLASS(VisualShaderNodeVectorScalarSmoothStep)
IMPL_GDCLASS(VisualShaderNodeVectorDistance)
IMPL_GDCLASS(VisualShaderNodeVectorRefract)
IMPL_GDCLASS(VisualShaderNodeScalarInterp)
IMPL_GDCLASS(VisualShaderNodeScalarSwitch)
IMPL_GDCLASS(VisualShaderNodeVectorInterp)
IMPL_GDCLASS(VisualShaderNodeVectorScalarMix)
IMPL_GDCLASS(VisualShaderNodeVectorCompose)
IMPL_GDCLASS(VisualShaderNodeTransformCompose)
IMPL_GDCLASS(VisualShaderNodeVectorDecompose)
IMPL_GDCLASS(VisualShaderNodeTransformDecompose)
IMPL_GDCLASS(VisualShaderNodeScalarUniform)
IMPL_GDCLASS(VisualShaderNodeBooleanUniform)
IMPL_GDCLASS(VisualShaderNodeColorUniform)
IMPL_GDCLASS(VisualShaderNodeVec3Uniform)
IMPL_GDCLASS(VisualShaderNodeTransformUniform)
IMPL_GDCLASS(VisualShaderNodeTextureUniform)
IMPL_GDCLASS(VisualShaderNodeTextureUniformTriplanar)
IMPL_GDCLASS(VisualShaderNodeCubeMapUniform)
IMPL_GDCLASS(VisualShaderNodeIf)
IMPL_GDCLASS(VisualShaderNodeSwitch)
IMPL_GDCLASS(VisualShaderNodeFresnel)
IMPL_GDCLASS(VisualShaderNodeIs)
IMPL_GDCLASS(VisualShaderNodeCompare)

VARIANT_ENUM_CAST(VisualShaderNodeTexture::TextureType)
VARIANT_ENUM_CAST(VisualShaderNodeTexture::Source)
VARIANT_ENUM_CAST(VisualShaderNodeCubeMap::Source)
VARIANT_ENUM_CAST(VisualShaderNodeCubeMap::TextureType)
VARIANT_ENUM_CAST(VisualShaderNodeScalarOp::Operator)
VARIANT_ENUM_CAST(VisualShaderNodeVectorOp::Operator)
VARIANT_ENUM_CAST(VisualShaderNodeColorOp::Operator)
VARIANT_ENUM_CAST(VisualShaderNodeTransformMult::Operator)
VARIANT_ENUM_CAST(VisualShaderNodeTransformVecMult::Operator)
VARIANT_ENUM_CAST(VisualShaderNodeScalarFunc::Function)
VARIANT_ENUM_CAST(VisualShaderNodeVectorFunc::Function)
VARIANT_ENUM_CAST(VisualShaderNodeColorFunc::Function)
VARIANT_ENUM_CAST(VisualShaderNodeTransformFunc::Function)
VARIANT_ENUM_CAST(VisualShaderNodeScalarDerivativeFunc::Function)
VARIANT_ENUM_CAST(VisualShaderNodeVectorDerivativeFunc::Function)
VARIANT_ENUM_CAST(VisualShaderNodeTextureUniform::TextureType)
VARIANT_ENUM_CAST(VisualShaderNodeTextureUniform::ColorDefault)
VARIANT_ENUM_CAST(VisualShaderNodeIs::Function)
VARIANT_ENUM_CAST(VisualShaderNodeCompare::ComparisonType)
VARIANT_ENUM_CAST(VisualShaderNodeCompare::Function)
VARIANT_ENUM_CAST(VisualShaderNodeCompare::Condition)

////////////// Scalar

se_string_view VisualShaderNodeScalarConstant::get_caption() const {
    return ("Scalar");
}

int VisualShaderNodeScalarConstant::get_input_port_count() const {
    return 0;
}
VisualShaderNodeScalarConstant::PortType VisualShaderNodeScalarConstant::get_input_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeScalarConstant::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeScalarConstant::get_output_port_count() const {
    return 1;
}
VisualShaderNodeScalarConstant::PortType VisualShaderNodeScalarConstant::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeScalarConstant::get_output_port_name(int p_port) const {
    return StringName(); //no output port means the editor will be used as port
}

String VisualShaderNodeScalarConstant::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = " + FormatVE("%.6f", constant) + ";\n";
}

void VisualShaderNodeScalarConstant::set_constant(float p_value) {

    constant = p_value;
    emit_changed();
}

float VisualShaderNodeScalarConstant::get_constant() const {

    return constant;
}

Vector<StringName> VisualShaderNodeScalarConstant::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("constant");
    return props;
}

void VisualShaderNodeScalarConstant::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_constant", {"value"}), &VisualShaderNodeScalarConstant::set_constant);
    MethodBinder::bind_method(D_METHOD("get_constant"), &VisualShaderNodeScalarConstant::get_constant);

    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeScalarConstant::VisualShaderNodeScalarConstant() {
    constant = 0;
}

////////////// Boolean

se_string_view VisualShaderNodeBooleanConstant::get_caption() const {
    return ("Boolean");
}

int VisualShaderNodeBooleanConstant::get_input_port_count() const {
    return 0;
}

VisualShaderNodeBooleanConstant::PortType VisualShaderNodeBooleanConstant::get_input_port_type(int p_port) const {
    return PORT_TYPE_BOOLEAN;
}

StringName VisualShaderNodeBooleanConstant::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeBooleanConstant::get_output_port_count() const {
    return 1;
}

VisualShaderNodeBooleanConstant::PortType VisualShaderNodeBooleanConstant::get_output_port_type(int p_port) const {
    return PORT_TYPE_BOOLEAN;
}

StringName VisualShaderNodeBooleanConstant::get_output_port_name(int p_port) const {
    return StringName(); //no output port means the editor will be used as port
}

String VisualShaderNodeBooleanConstant::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = " + (constant ? "true" : "false") + ";\n";
}

void VisualShaderNodeBooleanConstant::set_constant(bool p_value) {
    constant = p_value;
    emit_changed();
}

bool VisualShaderNodeBooleanConstant::get_constant() const {
    return constant;
}

Vector<StringName> VisualShaderNodeBooleanConstant::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("constant");
    return props;
}

void VisualShaderNodeBooleanConstant::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_constant", {"value"}), &VisualShaderNodeBooleanConstant::set_constant);
    MethodBinder::bind_method(D_METHOD("get_constant"), &VisualShaderNodeBooleanConstant::get_constant);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeBooleanConstant::VisualShaderNodeBooleanConstant() {
    constant = false;
}

////////////// Color

se_string_view VisualShaderNodeColorConstant::get_caption() const {
    return ("Color");
}

int VisualShaderNodeColorConstant::get_input_port_count() const {
    return 0;
}
VisualShaderNodeColorConstant::PortType VisualShaderNodeColorConstant::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeColorConstant::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeColorConstant::get_output_port_count() const {
    return 2;
}
VisualShaderNodeColorConstant::PortType VisualShaderNodeColorConstant::get_output_port_type(int p_port) const {
    return p_port == 0 ? PORT_TYPE_VECTOR : PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeColorConstant::get_output_port_name(int p_port) const {
    return StringName(p_port == 0 ? "" : "alpha"); //no output port means the editor will be used as port
}

String VisualShaderNodeColorConstant::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    String code;
    code += "\t" + p_output_vars[0] + " = " + FormatVE("vec3(%.6f,%.6f,%.6f)", constant.r, constant.g, constant.b) + ";\n";
    code += "\t" + p_output_vars[1] + " = " + FormatVE("%.6f", constant.a) + ";\n";

    return code;
}

void VisualShaderNodeColorConstant::set_constant(Color p_value) {

    constant = p_value;
    emit_changed();
}

Color VisualShaderNodeColorConstant::get_constant() const {

    return constant;
}

Vector<StringName> VisualShaderNodeColorConstant::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("constant");
    return props;
}

void VisualShaderNodeColorConstant::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_constant", {"value"}), &VisualShaderNodeColorConstant::set_constant);
    MethodBinder::bind_method(D_METHOD("get_constant"), &VisualShaderNodeColorConstant::get_constant);

    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeColorConstant::VisualShaderNodeColorConstant() {
    constant = Color(1, 1, 1, 1);
}

////////////// Vector

se_string_view VisualShaderNodeVec3Constant::get_caption() const {
    return ("Vector");
}

int VisualShaderNodeVec3Constant::get_input_port_count() const {
    return 0;
}
VisualShaderNodeVec3Constant::PortType VisualShaderNodeVec3Constant::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeVec3Constant::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeVec3Constant::get_output_port_count() const {
    return 1;
}
VisualShaderNodeVec3Constant::PortType VisualShaderNodeVec3Constant::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeVec3Constant::get_output_port_name(int p_port) const {
    return StringName(); //no output port means the editor will be used as port
}

String VisualShaderNodeVec3Constant::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = " + FormatVE(("vec3(%.6f,%.6f,%.6f)"), constant.x, constant.y, constant.z) + ";\n";
}

void VisualShaderNodeVec3Constant::set_constant(Vector3 p_value) {

    constant = p_value;
    emit_changed();
}

Vector3 VisualShaderNodeVec3Constant::get_constant() const {

    return constant;
}

Vector<StringName> VisualShaderNodeVec3Constant::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("constant");
    return props;
}

void VisualShaderNodeVec3Constant::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_constant", {"value"}), &VisualShaderNodeVec3Constant::set_constant);
    MethodBinder::bind_method(D_METHOD("get_constant"), &VisualShaderNodeVec3Constant::get_constant);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeVec3Constant::VisualShaderNodeVec3Constant() {
}

////////////// Transform

se_string_view VisualShaderNodeTransformConstant::get_caption() const {
    return "Transform";
}

int VisualShaderNodeTransformConstant::get_input_port_count() const {
    return 0;
}
VisualShaderNodeTransformConstant::PortType VisualShaderNodeTransformConstant::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeTransformConstant::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeTransformConstant::get_output_port_count() const {
    return 1;
}
VisualShaderNodeTransformConstant::PortType VisualShaderNodeTransformConstant::get_output_port_type(int p_port) const {
    return PORT_TYPE_TRANSFORM;
}
StringName VisualShaderNodeTransformConstant::get_output_port_name(int p_port) const {
    return StringName(); //no output port means the editor will be used as port
}

String VisualShaderNodeTransformConstant::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    Transform t = constant;
    t.basis.transpose();

    String code = "\t" + p_output_vars[0] + " = mat4(";
    code += FormatVE(("vec4(%.6f,%.6f,%.6f,0.0),"), t.basis[0].x, t.basis[0].y, t.basis[0].z);
    code += FormatVE(("vec4(%.6f,%.6f,%.6f,0.0),"), t.basis[1].x, t.basis[1].y, t.basis[1].z);
    code += FormatVE(("vec4(%.6f,%.6f,%.6f,0.0),"), t.basis[2].x, t.basis[2].y, t.basis[2].z);
    code += FormatVE(("vec4(%.6f,%.6f,%.6f,1.0) );\n"), t.origin.x, t.origin.y, t.origin.z);
    return code;
}

void VisualShaderNodeTransformConstant::set_constant(Transform p_value) {

    constant = p_value;
    emit_changed();
}

Transform VisualShaderNodeTransformConstant::get_constant() const {

    return constant;
}

Vector<StringName> VisualShaderNodeTransformConstant::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("constant");
    return props;
}

void VisualShaderNodeTransformConstant::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_constant", {"value"}), &VisualShaderNodeTransformConstant::set_constant);
    MethodBinder::bind_method(D_METHOD("get_constant"), &VisualShaderNodeTransformConstant::get_constant);

    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeTransformConstant::VisualShaderNodeTransformConstant() {
}

////////////// Texture

se_string_view VisualShaderNodeTexture::get_caption() const {
    return ("Texture");
}

int VisualShaderNodeTexture::get_input_port_count() const {
    return 3;
}
VisualShaderNodeTexture::PortType VisualShaderNodeTexture::get_input_port_type(int p_port) const {
    switch (p_port) {
        case 0:
            return PORT_TYPE_VECTOR;
        case 1:
            return PORT_TYPE_SCALAR;
        case 2:
            return PORT_TYPE_SAMPLER;
        default:
            return PORT_TYPE_SCALAR;
    }
}
StringName VisualShaderNodeTexture::get_input_port_name(int p_port) const {
    switch (p_port) {
        case 0:
            return ("uv");
        case 1:
            return ("lod");
        case 2:
            return ("sampler2D");
        default:
            return StringName();
    }
}

int VisualShaderNodeTexture::get_output_port_count() const {
    return 2;
}
VisualShaderNodeTexture::PortType VisualShaderNodeTexture::get_output_port_type(int p_port) const {
    if (p_port == 0 && source == SOURCE_DEPTH)
        return PORT_TYPE_SCALAR;
    return p_port == 0 ? PORT_TYPE_VECTOR : PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeTexture::get_output_port_name(int p_port) const {
    if (p_port == 0 && source == SOURCE_DEPTH)
        return StringName("depth");
    return StringName(p_port == 0 ? "rgb" : "alpha");
}

StringName VisualShaderNodeTexture::get_input_port_default_hint(int p_port) const {
    if (p_port == 0) {
        return StringName("UV.xy");
    }
    return StringName();
}

static String make_unique_id(VisualShader::Type p_type, int p_id, const String &p_name) {

    static const char *typepf[VisualShader::TYPE_MAX] = { "vtx", "frg", "lgt" };
    return p_name + "_" + (typepf[p_type]) + "_" + itos(p_id);
}

Vector<VisualShader::DefaultTextureParam> VisualShaderNodeTexture::get_default_texture_parameters(VisualShader::Type p_type, int p_id) const {
    VisualShader::DefaultTextureParam dtp;
    dtp.name = StringName(make_unique_id(p_type, p_id, "tex"));
    dtp.param = texture;
    Vector<VisualShader::DefaultTextureParam> ret;
    ret.push_back(dtp);
    return ret;
}

String VisualShaderNodeTexture::generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {

    if (source == SOURCE_TEXTURE) {

        String u("uniform sampler2D " + make_unique_id(p_type, p_id, "tex"));
        switch (texture_type) {
            case TYPE_DATA: break;
            case TYPE_COLOR: u += (" : hint_albedo"); break;
            case TYPE_NORMALMAP: u += (" : hint_normal"); break;
        }
        return u + ";";
    }

    return String();
}

String VisualShaderNodeTexture::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    if (source == SOURCE_TEXTURE) {
        String id = make_unique_id(p_type, p_id, "tex");
        String code;
        if (p_input_vars[0].empty()) { // Use UV by default.

            if (p_input_vars[1].empty()) {
                code += "\tvec4 " + id + "_read = texture( " + id + " , UV.xy );\n";
            } else {
                code += "\tvec4 " + id + "_read = textureLod( " + id + " , UV.xy , " + p_input_vars[1] + " );\n";
            }

        } else if (p_input_vars[1].empty()) {
            //no lod
            code += "\tvec4 " + id + "_read = texture( " + id + " , " + p_input_vars[0] + ".xy );\n";
        } else {
            code += "\tvec4 " + id + "_read = textureLod( " + id + " , " + p_input_vars[0] + ".xy , " + p_input_vars[1] + " );\n";
        }

        code += "\t" + p_output_vars[0] + " = " + id + "_read.rgb;\n";
        code += "\t" + p_output_vars[1] + " = " + id + "_read.a;\n";
        return code;
    }

    if (source == SOURCE_PORT) {
        String id = p_input_vars[2];

        String code;
        code += "\t{\n";
        if (id.empty()) {
            code += "\t\tvec4 " + id + "_tex_read = vec4(0.0);\n";
        } else {
            if (p_input_vars[0].empty()) { // Use UV by default.

                if (p_input_vars[1].empty()) {
                    code += "\t\tvec4 " + id + "_tex_read = texture( " + id + " , UV.xy );\n";
                } else {
                    code += "\t\tvec4 " + id + "_tex_read = textureLod( " + id + " , UV.xy , " + p_input_vars[1] + " );\n";
                }

            } else if (p_input_vars[1].empty()) {
                //no lod
                code += "\t\tvec4 " + id + "_tex_read = texture( " + id + " , " + p_input_vars[0] + ".xy );\n";
            } else {
                code += "\t\tvec4 " + id + "_tex_read = textureLod( " + id + " , " + p_input_vars[0] + ".xy , " + p_input_vars[1] + " );\n";
            }

            code += "\t\t" + p_output_vars[0] + " = " + id + "_tex_read.rgb;\n";
            code += "\t\t" + p_output_vars[1] + " = " + id + "_tex_read.a;\n";
        }
        code += "\t}\n";
        return code;
    }

    if (source == SOURCE_SCREEN && (p_mode == ShaderMode::SPATIAL || p_mode == ShaderMode::CANVAS_ITEM) && p_type == VisualShader::TYPE_FRAGMENT) {

        String code("\t{\n");
        if (p_input_vars[0].empty() || p_for_preview) { // Use UV by default.

            if (p_input_vars[1].empty()) {
                code += "\t\tvec4 _tex_read = textureLod( SCREEN_TEXTURE , UV.xy , 0.0 );\n";
            } else {
                code += "\t\tvec4 _tex_read = textureLod( SCREEN_TEXTURE , UV.xy , " + p_input_vars[1] + ");\n";
            }

        } else if (p_input_vars[1].empty()) {
            //no lod
            code += "\t\tvec4 _tex_read = textureLod( SCREEN_TEXTURE , " + p_input_vars[0] + ".xy, 0.0 );\n";
        } else {
            code += "\t\tvec4 _tex_read = textureLod( SCREEN_TEXTURE , " + p_input_vars[0] + ".xy , " + p_input_vars[1] + " );\n";
        }

        code += "\t\t" + p_output_vars[0] + " = _tex_read.rgb;\n";
        code += "\t\t" + p_output_vars[1] + " = _tex_read.a;\n";
        code += "\t}\n";
        return code;
    }

    if (source == SOURCE_2D_TEXTURE && p_mode == ShaderMode::CANVAS_ITEM && p_type == VisualShader::TYPE_FRAGMENT) {

        String code("\t{\n");
        if (p_input_vars[0].empty()) { // Use UV by default.

            if (p_input_vars->empty()) {
                code += ("\t\tvec4 _tex_read = texture( TEXTURE , UV.xy );\n");
            } else {
                code += "\t\tvec4 _tex_read = textureLod( TEXTURE , UV.xy , " + p_input_vars[1] + " );\n";
            }
        } else if (p_input_vars[1].empty()) {
            //no lod
            code += "\t\tvec4 _tex_read = texture( TEXTURE , " + p_input_vars[0] + ".xy );\n";
        } else {
            code += "\t\tvec4 _tex_read = textureLod( TEXTURE , " + p_input_vars[0] + ".xy , " + p_input_vars[1] + " );\n";
        }

        code += "\t\t" + p_output_vars[0] + " = _tex_read.rgb;\n";
        code += "\t\t" + p_output_vars[1] + " = _tex_read.a;\n";
        code += ("\t}\n");
        return code;
    }

    if (source == SOURCE_2D_NORMAL && p_mode == ShaderMode::CANVAS_ITEM && p_type == VisualShader::TYPE_FRAGMENT) {

        String code("\t{\n");
        if (p_input_vars[0].empty()) { // Use UV by default.

            if (p_input_vars[1].empty()) {
                code += ("\t\tvec4 _tex_read = texture( NORMAL_TEXTURE , UV.xy );\n");
            } else {
                code += "\t\tvec4 _tex_read = textureLod( NORMAL_TEXTURE , UV.xy , " + p_input_vars[1] + " );\n";
            }
        } else if (p_input_vars[1].empty()) {
            //no lod
            code += "\t\tvec4 _tex_read = texture( NORMAL_TEXTURE , " + p_input_vars[0] + ".xy );\n";
        } else {
            code += "\t\tvec4 _tex_read = textureLod( NORMAL_TEXTURE , " + p_input_vars[0] + ".xy , " + p_input_vars[1] + " );\n";
        }

        code += "\t\t" + p_output_vars[0] + " = _tex_read.rgb;\n";
        code += "\t\t" + p_output_vars[1] + " = _tex_read.a;\n";
        code += ("\t}\n");
        return code;
    }

    if (p_for_preview) // DEPTH_TEXTURE is not supported in preview(canvas_item) shader
    {
        if (source == SOURCE_DEPTH) {
            String code;
            code += "\t" + p_output_vars[0] + " = 0.0;\n";
            code += "\t" + p_output_vars[1] + " = 1.0;\n";
            return code;
        }
    }

    if (source == SOURCE_DEPTH && p_mode == ShaderMode::SPATIAL && p_type == VisualShader::TYPE_FRAGMENT) {

        String code("\t{\n");
        if (p_input_vars[0].empty()) { // Use UV by default.

            if (p_input_vars[1].empty()) {
                code += ("\t\tfloat _depth = texture( DEPTH_TEXTURE , UV.xy ).r;\n");
            } else {
                code += "\t\tfloat _depth = textureLod( DEPTH_TEXTURE , UV.xy , " + p_input_vars[1] + " ).r;\n";
            }
        } else if (p_input_vars[1].empty()) {
            //no lod
            code += "\t\tfloat _depth = texture( DEPTH_TEXTURE , " + p_input_vars[0] + ".xy ).r;\n";
        } else {
            code += "\t\tfloat _depth = textureLod( DEPTH_TEXTURE , " + p_input_vars[0] + ".xy , " + p_input_vars[1] + " ).r;\n";
        }

        code += "\t\t" + p_output_vars[0] + " = _depth;\n";
        code += "\t\t" + p_output_vars[1] + " = 1.0;\n";
        code += ("\t}\n");
        return code;
    } else if (source == SOURCE_DEPTH) {
        String code;
        code += "\t" + p_output_vars[0] + " = 0.0;\n";
        code += "\t" + p_output_vars[1] + " = 1.0;\n";
        return code;
    }

    //none
    String code;
    code += "\t" + p_output_vars[0] + " = vec3(0.0);\n";
    code += "\t" + p_output_vars[1] + " = 1.0;\n";
    return code;
}

void VisualShaderNodeTexture::set_source(Source p_source) {
    source = p_source;
    emit_changed();
    emit_signal("editor_refresh_request");
}

VisualShaderNodeTexture::Source VisualShaderNodeTexture::get_source() const {
    return source;
}

void VisualShaderNodeTexture::set_texture(Ref<Texture> p_value) {

    texture = p_value;
    emit_changed();
}

Ref<Texture> VisualShaderNodeTexture::get_texture() const {

    return texture;
}

void VisualShaderNodeTexture::set_texture_type(TextureType p_type) {
    texture_type = p_type;
    emit_changed();
}

VisualShaderNodeTexture::TextureType VisualShaderNodeTexture::get_texture_type() const {
    return texture_type;
}

Vector<StringName> VisualShaderNodeTexture::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("source");
    if (source == SOURCE_TEXTURE) {
        props.push_back("texture");
        props.push_back("texture_type");
    }
    return props;
}

StringName VisualShaderNodeTexture::get_warning(ShaderMode p_mode, VisualShader::Type p_type) const {

    if (source == SOURCE_TEXTURE) {
        return StringName(); // all good
    }

    if (source == SOURCE_PORT) {
        return StringName(); // all good
    }
    if (source == SOURCE_SCREEN && (p_mode == ShaderMode::SPATIAL || p_mode == ShaderMode::CANVAS_ITEM) && p_type == VisualShader::TYPE_FRAGMENT) {

        return StringName(); // all good
    }

    if (source == SOURCE_2D_TEXTURE && p_mode == ShaderMode::CANVAS_ITEM && p_type == VisualShader::TYPE_FRAGMENT) {

        return StringName(); // all good
    }

    if (source == SOURCE_2D_NORMAL && p_mode == ShaderMode::CANVAS_ITEM) {

        return StringName(); // all good
    }

    if (source == SOURCE_DEPTH && p_mode == ShaderMode::SPATIAL && p_type == VisualShader::TYPE_FRAGMENT) {

        if (get_output_port_for_preview() == 0) { // DEPTH_TEXTURE is not supported in preview(canvas_item) shader
            return TTR("Invalid source for preview.");
        }
        return StringName(); // all good
    }

    return TTR("Invalid source for shader.");
}

void VisualShaderNodeTexture::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_source", {"value"}), &VisualShaderNodeTexture::set_source);
    MethodBinder::bind_method(D_METHOD("get_source"), &VisualShaderNodeTexture::get_source);

    MethodBinder::bind_method(D_METHOD("set_texture", {"value"}), &VisualShaderNodeTexture::set_texture);
    MethodBinder::bind_method(D_METHOD("get_texture"), &VisualShaderNodeTexture::get_texture);

    MethodBinder::bind_method(D_METHOD("set_texture_type", {"value"}), &VisualShaderNodeTexture::set_texture_type);
    MethodBinder::bind_method(D_METHOD("get_texture_type"), &VisualShaderNodeTexture::get_texture_type);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "source", PropertyHint::Enum, "Texture,Screen,Texture2D,NormalMap2D,Depth,SamplerPort"), "set_source", "get_source");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "texture_type", PropertyHint::Enum, "Data,Color,Normalmap"), "set_texture_type", "get_texture_type");

    BIND_ENUM_CONSTANT(SOURCE_TEXTURE)
    BIND_ENUM_CONSTANT(SOURCE_SCREEN)
    BIND_ENUM_CONSTANT(SOURCE_2D_TEXTURE)
    BIND_ENUM_CONSTANT(SOURCE_2D_NORMAL)
    BIND_ENUM_CONSTANT(SOURCE_DEPTH)
    BIND_ENUM_CONSTANT(SOURCE_PORT)
    BIND_ENUM_CONSTANT(TYPE_DATA)
    BIND_ENUM_CONSTANT(TYPE_COLOR)
    BIND_ENUM_CONSTANT(TYPE_NORMALMAP)
}

VisualShaderNodeTexture::VisualShaderNodeTexture() {
    texture_type = TYPE_DATA;
    source = SOURCE_TEXTURE;
}

////////////// CubeMap

se_string_view VisualShaderNodeCubeMap::get_caption() const {
    return ("CubeMap");
}

int VisualShaderNodeCubeMap::get_input_port_count() const {
    return 3;
}
VisualShaderNodeCubeMap::PortType VisualShaderNodeCubeMap::get_input_port_type(int p_port) const {
    switch (p_port) {
        case 0: return PORT_TYPE_VECTOR;
        case 1: return PORT_TYPE_SCALAR;
        case 2: return PORT_TYPE_SAMPLER;
        default: return PORT_TYPE_SCALAR;
    }
}

StringName VisualShaderNodeCubeMap::get_input_port_name(int p_port) const {
    switch (p_port) {
        case 0: return StringName("uv");
        case 1: return StringName("lod");
        case 2: return StringName("samplerCube");
        default: return StringName();
    }
}

int VisualShaderNodeCubeMap::get_output_port_count() const {
    return 2;
}
VisualShaderNodeCubeMap::PortType VisualShaderNodeCubeMap::get_output_port_type(int p_port) const {
    return p_port == 0 ? PORT_TYPE_VECTOR : PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeCubeMap::get_output_port_name(int p_port) const {
    return StringName(p_port == 0 ? "rgb" : "alpha");
}

Vector<VisualShader::DefaultTextureParam> VisualShaderNodeCubeMap::get_default_texture_parameters(VisualShader::Type p_type, int p_id) const {
    VisualShader::DefaultTextureParam dtp;
    dtp.name = StringName(make_unique_id(p_type, p_id, ("cube")));
    dtp.param = cube_map;
    Vector<VisualShader::DefaultTextureParam> ret;
    ret.push_back(dtp);
    return ret;
}

String VisualShaderNodeCubeMap::generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {

    if (source == SOURCE_TEXTURE) {
        String u = "uniform samplerCube " + make_unique_id(p_type, p_id, ("cube"));
        switch (texture_type) {
            case TYPE_DATA: break;
            case TYPE_COLOR: u += (" : hint_albedo"); break;
            case TYPE_NORMALMAP: u += (" : hint_normal"); break;
        }
        return u + ";\n";
    }
    return String();
}

String VisualShaderNodeCubeMap::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    String code;
    String id;
    if (source == SOURCE_TEXTURE) {
        id = make_unique_id(p_type, p_id, ("cube"));
    } else if (source == SOURCE_PORT) {
        id = p_input_vars[2];
    } else {
        return String();
    }
    code += "\t{\n";

    if (id.empty()) {
        code += "\t\tvec4 " + id + "_read = vec4(0.0);\n";
        code += "\t\t" + p_output_vars[0] + " = " + id + "_read.rgb;\n";
        code += "\t\t" + p_output_vars[1] + " = " + id + "_read.a;\n";
        return code;
    }

    if (p_input_vars[0].empty()) { // Use UV by default.

        if (p_input_vars[1].empty()) {
            code += "\t\tvec4 " + id + "_read = texture( " + id + " , vec3( UV, 0.0 ) );\n";
        } else {
            code += "\t\tvec4 " + id + "_read = textureLod( " + id + " , vec3( UV, 0.0 )" + " , " + p_input_vars[1] + " );\n";
        }

    } else if (p_input_vars[1].empty()) {
        //no lod
        code += "\t\tvec4 " + id + "_read = texture( " + id + " , " + p_input_vars[0] + " );\n";
    } else {
        code += "\t\tvec4 " + id + "_read = textureLod( " + id + " , " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n";
    }
    code += "\t\t" + p_output_vars[0] + " = " + id + "_read.rgb;\n";
    code += "\t\t" + p_output_vars[1] + " = " + id + "_read.a;\n";
    code += "\t}\n";

    return code;
}
StringName VisualShaderNodeCubeMap::get_input_port_default_hint(int p_port) const {
    if (p_port == 0) {
        return StringName("vec3(UV, 0.0)");
    }
    return StringName();
}

void VisualShaderNodeCubeMap::set_source(Source p_source) {
    source = p_source;
    emit_changed();
    emit_signal("editor_refresh_request");
}

VisualShaderNodeCubeMap::Source VisualShaderNodeCubeMap::get_source() const {
    return source;
}

void VisualShaderNodeCubeMap::set_cube_map(Ref<CubeMap> p_value) {

    cube_map = p_value;
    emit_changed();
}

Ref<CubeMap> VisualShaderNodeCubeMap::get_cube_map() const {

    return cube_map;
}

void VisualShaderNodeCubeMap::set_texture_type(TextureType p_type) {
    texture_type = p_type;
    emit_changed();
}

VisualShaderNodeCubeMap::TextureType VisualShaderNodeCubeMap::get_texture_type() const {
    return texture_type;
}

Vector<StringName> VisualShaderNodeCubeMap::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("source");
    if (source == SOURCE_TEXTURE) {
        props.push_back("cube_map");
        props.push_back("texture_type");
    }
    return props;
}
void VisualShaderNodeCubeMap::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("set_source", {"value"}), &VisualShaderNodeCubeMap::set_source);
    MethodBinder::bind_method(D_METHOD("get_source"), &VisualShaderNodeCubeMap::get_source);

    MethodBinder::bind_method(D_METHOD("set_cube_map", {"value"}), &VisualShaderNodeCubeMap::set_cube_map);
    MethodBinder::bind_method(D_METHOD("get_cube_map"), &VisualShaderNodeCubeMap::get_cube_map);

    MethodBinder::bind_method(D_METHOD("set_texture_type", {"value"}), &VisualShaderNodeCubeMap::set_texture_type);
    MethodBinder::bind_method(D_METHOD("get_texture_type"), &VisualShaderNodeCubeMap::get_texture_type);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "source", PropertyHint::Enum, "Texture,SamplerPort"), "set_source", "get_source");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "cube_map", PropertyHint::ResourceType, "CubeMap"), "set_cube_map", "get_cube_map");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "texture_type", PropertyHint::Enum, "Data,Color,Normalmap"), "set_texture_type", "get_texture_type");

    BIND_ENUM_CONSTANT(SOURCE_TEXTURE);
    BIND_ENUM_CONSTANT(SOURCE_PORT);

    BIND_ENUM_CONSTANT(TYPE_DATA)
    BIND_ENUM_CONSTANT(TYPE_COLOR)
    BIND_ENUM_CONSTANT(TYPE_NORMALMAP)
}

VisualShaderNodeCubeMap::VisualShaderNodeCubeMap() {
    texture_type = TYPE_DATA;
    source = SOURCE_TEXTURE;
}
////////////// Scalar Op

se_string_view VisualShaderNodeScalarOp::get_caption() const {
    return ("ScalarOp");
}

int VisualShaderNodeScalarOp::get_input_port_count() const {
    return 2;
}
VisualShaderNodeScalarOp::PortType VisualShaderNodeScalarOp::get_input_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeScalarOp::get_input_port_name(int p_port) const {
    return StringName(p_port == 0 ? "a" : "b");
}

int VisualShaderNodeScalarOp::get_output_port_count() const {
    return 1;
}
VisualShaderNodeScalarOp::PortType VisualShaderNodeScalarOp::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeScalarOp::get_output_port_name(int p_port) const {
    return StringName("op"); //no output port means the editor will be used as port
}

String VisualShaderNodeScalarOp::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    String code = "\t" + p_output_vars[0] + " = ";
    switch (op) {

        case OP_ADD: code += p_input_vars[0] + " + " + p_input_vars[1] + ";\n"; break;
        case OP_SUB: code += p_input_vars[0] + " - " + p_input_vars[1] + ";\n"; break;
        case OP_MUL: code += p_input_vars[0] + " * " + p_input_vars[1] + ";\n"; break;
        case OP_DIV: code += p_input_vars[0] + " / " + p_input_vars[1] + ";\n"; break;
        case OP_MOD: code += "mod( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
        case OP_POW: code += "pow( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
        case OP_MAX: code += "max( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
        case OP_MIN: code += "min( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
        case OP_ATAN2: code += "atan( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
        case OP_STEP: code += "step( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
    }

    return code;
}

void VisualShaderNodeScalarOp::set_operator(Operator p_op) {

    op = p_op;
    emit_changed();
}

VisualShaderNodeScalarOp::Operator VisualShaderNodeScalarOp::get_operator() const {

    return op;
}

Vector<StringName> VisualShaderNodeScalarOp::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("operator");
    return props;
}

void VisualShaderNodeScalarOp::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_operator", {"op"}), &VisualShaderNodeScalarOp::set_operator);
    MethodBinder::bind_method(D_METHOD("get_operator"), &VisualShaderNodeScalarOp::get_operator);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "operator", PropertyHint::Enum, "Add,Sub,Multiply,Divide,Remainder,Power,Max,Min,Atan2,Step"), "set_operator", "get_operator");

    BIND_ENUM_CONSTANT(OP_ADD)
    BIND_ENUM_CONSTANT(OP_SUB)
    BIND_ENUM_CONSTANT(OP_MUL)
    BIND_ENUM_CONSTANT(OP_DIV)
    BIND_ENUM_CONSTANT(OP_MOD)
    BIND_ENUM_CONSTANT(OP_POW)
    BIND_ENUM_CONSTANT(OP_MAX)
    BIND_ENUM_CONSTANT(OP_MIN)
    BIND_ENUM_CONSTANT(OP_ATAN2)
    BIND_ENUM_CONSTANT(OP_STEP)
}

VisualShaderNodeScalarOp::VisualShaderNodeScalarOp() {
    op = OP_ADD;
    set_input_port_default_value(0, 0.0);
    set_input_port_default_value(1, 0.0);
}

////////////// Vector Op

se_string_view VisualShaderNodeVectorOp::get_caption() const {
    return ("VectorOp");
}

int VisualShaderNodeVectorOp::get_input_port_count() const {
    return 2;
}
VisualShaderNodeVectorOp::PortType VisualShaderNodeVectorOp::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeVectorOp::get_input_port_name(int p_port) const {
    return StringName(p_port == 0 ? "a" : "b");
}

int VisualShaderNodeVectorOp::get_output_port_count() const {
    return 1;
}
VisualShaderNodeVectorOp::PortType VisualShaderNodeVectorOp::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeVectorOp::get_output_port_name(int p_port) const {
    return StringName("op"); //no output port means the editor will be used as port
}

String VisualShaderNodeVectorOp::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    String code = "\t" + p_output_vars[0] + " = ";
    switch (op) {

        case OP_ADD: code += p_input_vars[0] + " + " + p_input_vars[1] + ";\n"; break;
        case OP_SUB: code += p_input_vars[0] + " - " + p_input_vars[1] + ";\n"; break;
        case OP_MUL: code += p_input_vars[0] + " * " + p_input_vars[1] + ";\n"; break;
        case OP_DIV: code += p_input_vars[0] + " / " + p_input_vars[1] + ";\n"; break;
        case OP_MOD: code += "mod( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
        case OP_POW: code += "pow( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
        case OP_MAX: code += "max( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
        case OP_MIN: code += "min( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
        case OP_CROSS: code += "cross( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
        case OP_ATAN2: code += "atan( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
        case OP_REFLECT: code += "reflect( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
        case OP_STEP: code += "step( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n"; break;
    }

    return code;
}

void VisualShaderNodeVectorOp::set_operator(Operator p_op) {

    op = p_op;
    emit_changed();
}

VisualShaderNodeVectorOp::Operator VisualShaderNodeVectorOp::get_operator() const {

    return op;
}

Vector<StringName> VisualShaderNodeVectorOp::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("operator");
    return props;
}

void VisualShaderNodeVectorOp::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_operator", {"op"}), &VisualShaderNodeVectorOp::set_operator);
    MethodBinder::bind_method(D_METHOD("get_operator"), &VisualShaderNodeVectorOp::get_operator);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "operator", PropertyHint::Enum, "Add,Sub,Multiply,Divide,Remainder,Power,Max,Min,Cross,Atan2,Reflect,Step"), "set_operator", "get_operator");

    BIND_ENUM_CONSTANT(OP_ADD)
    BIND_ENUM_CONSTANT(OP_SUB)
    BIND_ENUM_CONSTANT(OP_MUL)
    BIND_ENUM_CONSTANT(OP_DIV)
    BIND_ENUM_CONSTANT(OP_MOD)
    BIND_ENUM_CONSTANT(OP_POW)
    BIND_ENUM_CONSTANT(OP_MAX)
    BIND_ENUM_CONSTANT(OP_MIN)
    BIND_ENUM_CONSTANT(OP_CROSS)
    BIND_ENUM_CONSTANT(OP_ATAN2)
    BIND_ENUM_CONSTANT(OP_REFLECT)
    BIND_ENUM_CONSTANT(OP_STEP)
}

VisualShaderNodeVectorOp::VisualShaderNodeVectorOp() {
    op = OP_ADD;
    set_input_port_default_value(0, Vector3());
    set_input_port_default_value(1, Vector3());
}

////////////// Color Op

se_string_view VisualShaderNodeColorOp::get_caption() const {
    return ("ColorOp");
}

int VisualShaderNodeColorOp::get_input_port_count() const {
    return 2;
}
VisualShaderNodeColorOp::PortType VisualShaderNodeColorOp::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeColorOp::get_input_port_name(int p_port) const {
    return StringName(p_port == 0 ? "a" : "b");
}

int VisualShaderNodeColorOp::get_output_port_count() const {
    return 1;
}
VisualShaderNodeColorOp::PortType VisualShaderNodeColorOp::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeColorOp::get_output_port_name(int p_port) const {
    return StringName("op"); //no output port means the editor will be used as port
}

String VisualShaderNodeColorOp::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    String code;
    static const char *axisn[3] = { "x", "y", "z" };
    switch (op) {
        case OP_SCREEN: {

            code += "\t" + p_output_vars[0] + "=vec3(1.0)-(vec3(1.0)-" + p_input_vars[0] + ")*(vec3(1.0)-" + p_input_vars[1] + ");\n";
        } break;
        case OP_DIFFERENCE: {

            code += "\t" + p_output_vars[0] + "=abs(" + p_input_vars[0] + "-" + p_input_vars[1] + ");\n";
        } break;
        case OP_DARKEN: {

            code += "\t" + p_output_vars[0] + "=min(" + p_input_vars[0] + "," + p_input_vars[1] + ");\n";
        } break;
        case OP_LIGHTEN: {

            code += "\t" + p_output_vars[0] + "=max(" + p_input_vars[0] + "," + p_input_vars[1] + ");\n";

        } break;
        case OP_OVERLAY: {

            for (int i = 0; i < 3; i++) {
                code += ("\t{\n");
                code += "\t\tfloat base=" + p_input_vars[0] + "." + axisn[i] + ";\n";
                code += "\t\tfloat blend=" + p_input_vars[1] + "." + axisn[i] + ";\n";
                code += ("\t\tif (base < 0.5) {\n");
                code += "\t\t\t" + p_output_vars[0] + "." + axisn[i] + " = 2.0 * base * blend;\n";
                code += ("\t\t} else {\n");
                code += "\t\t\t" + p_output_vars[0] + "." + axisn[i] + " = 1.0 - 2.0 * (1.0 - blend) * (1.0 - base);\n";
                code += ("\t\t}\n");
                code += ("\t}\n");
            }

        } break;
        case OP_DODGE: {

            code += "\t" + p_output_vars[0] + "=(" + p_input_vars[0] + ")/(vec3(1.0)-" + p_input_vars[1] + ");\n";

        } break;
        case OP_BURN: {

            code += "\t" + p_output_vars[0] + "=vec3(1.0)-(vec3(1.0)-" + p_input_vars[0] + ")/(" + p_input_vars[1] + ");\n";
        } break;
        case OP_SOFT_LIGHT: {

            for (int i = 0; i < 3; i++) {
                code += ("\t{\n");
                code += "\t\tfloat base=" + p_input_vars[0] + "." + axisn[i] + ";\n";
                code += "\t\tfloat blend=" + p_input_vars[1] + "." + axisn[i] + ";\n";
                code += ("\t\tif (base < 0.5) {\n");
                code += "\t\t\t" + p_output_vars[0] + "." + axisn[i] + " = (base * (blend+0.5));\n";
                code += ("\t\t} else {\n");
                code += "\t\t\t" + p_output_vars[0] + "." + axisn[i] + " = (1.0 - (1.0-base) * (1.0-(blend-0.5)));\n";
                code += ("\t\t}\n");
                code += ("\t}\n");
            }

        } break;
        case OP_HARD_LIGHT: {

            for (int i = 0; i < 3; i++) {
                code += ("\t{\n");
                code += "\t\tfloat base=" + p_input_vars[0] + "." + axisn[i] + ";\n";
                code += "\t\tfloat blend=" + p_input_vars[1] + "." + axisn[i] + ";\n";
                code += ("\t\tif (base < 0.5) {\n");
                code += "\t\t\t" + p_output_vars[0] + "." + axisn[i] + " = (base * (2.0*blend));\n";
                code += ("\t\t} else {\n");
                code += "\t\t\t" + p_output_vars[0] + "." + axisn[i] + " = (1.0 - (1.0-base) * (1.0-2.0*(blend-0.5)));\n";
                code += ("\t\t}\n");
                code += ("\t}\n");
            }

        } break;
    }

    return code;
}

void VisualShaderNodeColorOp::set_operator(Operator p_op) {

    op = p_op;
    emit_changed();
}

VisualShaderNodeColorOp::Operator VisualShaderNodeColorOp::get_operator() const {

    return op;
}

Vector<StringName> VisualShaderNodeColorOp::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("operator");
    return props;
}

void VisualShaderNodeColorOp::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_operator", {"op"}), &VisualShaderNodeColorOp::set_operator);
    MethodBinder::bind_method(D_METHOD("get_operator"), &VisualShaderNodeColorOp::get_operator);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "operator", PropertyHint::Enum, "Screen,Difference,Darken,Lighten,Overlay,Dodge,Burn,SoftLight,HardLight"), "set_operator", "get_operator");

    BIND_ENUM_CONSTANT(OP_SCREEN)
    BIND_ENUM_CONSTANT(OP_DIFFERENCE)
    BIND_ENUM_CONSTANT(OP_DARKEN)
    BIND_ENUM_CONSTANT(OP_LIGHTEN)
    BIND_ENUM_CONSTANT(OP_OVERLAY)
    BIND_ENUM_CONSTANT(OP_DODGE)
    BIND_ENUM_CONSTANT(OP_BURN)
    BIND_ENUM_CONSTANT(OP_SOFT_LIGHT)
    BIND_ENUM_CONSTANT(OP_HARD_LIGHT)
}

VisualShaderNodeColorOp::VisualShaderNodeColorOp() {
    op = OP_SCREEN;
    set_input_port_default_value(0, Vector3());
    set_input_port_default_value(1, Vector3());
}

////////////// Transform Mult

se_string_view VisualShaderNodeTransformMult::get_caption() const {
    return ("TransformMult");
}

int VisualShaderNodeTransformMult::get_input_port_count() const {
    return 2;
}
VisualShaderNodeTransformMult::PortType VisualShaderNodeTransformMult::get_input_port_type(int p_port) const {
    return PORT_TYPE_TRANSFORM;
}
StringName VisualShaderNodeTransformMult::get_input_port_name(int p_port) const {
    return StringName(p_port == 0 ? "a" : "b");
}

int VisualShaderNodeTransformMult::get_output_port_count() const {
    return 1;
}
VisualShaderNodeTransformMult::PortType VisualShaderNodeTransformMult::get_output_port_type(int p_port) const {
    return PORT_TYPE_TRANSFORM;
}
StringName VisualShaderNodeTransformMult::get_output_port_name(int p_port) const {
    return StringName("mult"); //no output port means the editor will be used as port
}

String VisualShaderNodeTransformMult::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    if (op == OP_AxB) {
        return "\t" + p_output_vars[0] + " = " + p_input_vars[0] + " * " + p_input_vars[1] + ";\n";
    } else if (op == OP_BxA) {
        return "\t" + p_output_vars[0] + " = " + p_input_vars[1] + " * " + p_input_vars[0] + ";\n";
    } else if (op == OP_AxB_COMP) {
        return "\t" + p_output_vars[0] + " = matrixCompMult( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n";
    } else {
        return "\t" + p_output_vars[0] + " = matrixCompMult( " + p_input_vars[1] + " , " + p_input_vars[0] + " );\n";
    }
}

void VisualShaderNodeTransformMult::set_operator(Operator p_op) {

    op = p_op;
    emit_changed();
}

VisualShaderNodeTransformMult::Operator VisualShaderNodeTransformMult::get_operator() const {

    return op;
}

Vector<StringName> VisualShaderNodeTransformMult::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("operator");
    return props;
}

void VisualShaderNodeTransformMult::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_operator", {"op"}), &VisualShaderNodeTransformMult::set_operator);
    MethodBinder::bind_method(D_METHOD("get_operator"), &VisualShaderNodeTransformMult::get_operator);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "operator", PropertyHint::Enum, "A x B,B x A,A x B(per component),B x A(per component)"), "set_operator", "get_operator");

    BIND_ENUM_CONSTANT(OP_AxB)
    BIND_ENUM_CONSTANT(OP_BxA)
    BIND_ENUM_CONSTANT(OP_AxB_COMP)
    BIND_ENUM_CONSTANT(OP_BxA_COMP)
}

VisualShaderNodeTransformMult::VisualShaderNodeTransformMult() {
    op = OP_AxB;
    set_input_port_default_value(0, Transform());
    set_input_port_default_value(1, Transform());
}

////////////// TransformVec Mult

se_string_view VisualShaderNodeTransformVecMult::get_caption() const {
    return ("TransformVectorMult");
}

int VisualShaderNodeTransformVecMult::get_input_port_count() const {
    return 2;
}
VisualShaderNodeTransformVecMult::PortType VisualShaderNodeTransformVecMult::get_input_port_type(int p_port) const {
    return p_port == 0 ? PORT_TYPE_TRANSFORM : PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeTransformVecMult::get_input_port_name(int p_port) const {
    return StringName(p_port == 0 ? "a" : "b");
}

int VisualShaderNodeTransformVecMult::get_output_port_count() const {
    return 1;
}
VisualShaderNodeTransformVecMult::PortType VisualShaderNodeTransformVecMult::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeTransformVecMult::get_output_port_name(int p_port) const {
    return StringName(); //no output port means the editor will be used as port
}

String VisualShaderNodeTransformVecMult::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    if (op == OP_AxB) {
        return "\t" + p_output_vars[0] + " = ( " + p_input_vars[0] + " * vec4(" + p_input_vars[1] + ", 1.0) ).xyz;\n";
    } else if (op == OP_BxA) {
        return "\t" + p_output_vars[0] + " = ( vec4(" + p_input_vars[1] + ", 1.0) * " + p_input_vars[0] + " ).xyz;\n";
    } else if (op == OP_3x3_AxB) {
        return "\t" + p_output_vars[0] + " = ( " + p_input_vars[0] + " * vec4(" + p_input_vars[1] + ", 0.0) ).xyz;\n";
    } else {
        return "\t" + p_output_vars[0] + " = ( vec4(" + p_input_vars[1] + ", 0.0) * " + p_input_vars[0] + " ).xyz;\n";
    }
}

void VisualShaderNodeTransformVecMult::set_operator(Operator p_op) {

    op = p_op;
    emit_changed();
}

VisualShaderNodeTransformVecMult::Operator VisualShaderNodeTransformVecMult::get_operator() const {

    return op;
}

Vector<StringName> VisualShaderNodeTransformVecMult::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("operator");
    return props;
}

void VisualShaderNodeTransformVecMult::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_operator", {"op"}), &VisualShaderNodeTransformVecMult::set_operator);
    MethodBinder::bind_method(D_METHOD("get_operator"), &VisualShaderNodeTransformVecMult::get_operator);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "operator", PropertyHint::Enum, "A x B,B x A,A x B (3x3),B x A (3x3)"), "set_operator", "get_operator");

    BIND_ENUM_CONSTANT(OP_AxB)
    BIND_ENUM_CONSTANT(OP_BxA)
    BIND_ENUM_CONSTANT(OP_3x3_AxB)
    BIND_ENUM_CONSTANT(OP_3x3_BxA)
}

VisualShaderNodeTransformVecMult::VisualShaderNodeTransformVecMult() {
    op = OP_AxB;
    set_input_port_default_value(0, Transform());
    set_input_port_default_value(1, Vector3());
}

////////////// Scalar Func

se_string_view VisualShaderNodeScalarFunc::get_caption() const {
    return ("ScalarFunc");
}

int VisualShaderNodeScalarFunc::get_input_port_count() const {
    return 1;
}
VisualShaderNodeScalarFunc::PortType VisualShaderNodeScalarFunc::get_input_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeScalarFunc::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeScalarFunc::get_output_port_count() const {
    return 1;
}
VisualShaderNodeScalarFunc::PortType VisualShaderNodeScalarFunc::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeScalarFunc::get_output_port_name(int p_port) const {
    return StringName(); //no output port means the editor will be used as port
}

String VisualShaderNodeScalarFunc::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    static const char *scalar_func_id[FUNC_ONEMINUS + 1] = {
        "sin($)",
        "cos($)",
        "tan($)",
        "asin($)",
        "acos($)",
        "atan($)",
        "sinh($)",
        "cosh($)",
        "tanh($)",
        "log($)",
        "exp($)",
        "sqrt($)",
        "abs($)",
        "sign($)",
        "floor($)",
        "round($)",
        "ceil($)",
        "fract($)",
        "min(max($,0.0),1.0)",
        "-($)",
        "acosh($)",
        "asinh($)",
        "atanh($)",
        "degrees($)",
        "exp2($)",
        "inversesqrt($)",
        "log2($)",
        "radians($)",
        "1.0/($)",
        "roundEven($)",
        "trunc($)",
        "1.0-$"
    };

    return "\t" + p_output_vars[0] + " = " + StringUtils::replace(String(scalar_func_id[func]),String("$"), p_input_vars[0]) + ";\n";
}

void VisualShaderNodeScalarFunc::set_function(Function p_func) {

    func = p_func;
    emit_changed();
}

VisualShaderNodeScalarFunc::Function VisualShaderNodeScalarFunc::get_function() const {

    return func;
}

Vector<StringName> VisualShaderNodeScalarFunc::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("function");
    return props;
}

void VisualShaderNodeScalarFunc::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_function", {"func"}), &VisualShaderNodeScalarFunc::set_function);
    MethodBinder::bind_method(D_METHOD("get_function"), &VisualShaderNodeScalarFunc::get_function);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "function", PropertyHint::Enum, "Sin,Cos,Tan,ASin,ACos,ATan,SinH,CosH,TanH,Log,Exp,Sqrt,Abs,Sign,Floor,Round,Ceil,Frac,Saturate,Negate,ACosH,ASinH,ATanH,Degrees,Exp2,InverseSqrt,Log2,Radians,Reciprocal,RoundEven,Trunc,OneMinus"), "set_function", "get_function");

    BIND_ENUM_CONSTANT(FUNC_SIN)
    BIND_ENUM_CONSTANT(FUNC_COS)
    BIND_ENUM_CONSTANT(FUNC_TAN)
    BIND_ENUM_CONSTANT(FUNC_ASIN)
    BIND_ENUM_CONSTANT(FUNC_ACOS)
    BIND_ENUM_CONSTANT(FUNC_ATAN)
    BIND_ENUM_CONSTANT(FUNC_SINH)
    BIND_ENUM_CONSTANT(FUNC_COSH)
    BIND_ENUM_CONSTANT(FUNC_TANH)
    BIND_ENUM_CONSTANT(FUNC_LOG)
    BIND_ENUM_CONSTANT(FUNC_EXP)
    BIND_ENUM_CONSTANT(FUNC_SQRT)
    BIND_ENUM_CONSTANT(FUNC_ABS)
    BIND_ENUM_CONSTANT(FUNC_SIGN)
    BIND_ENUM_CONSTANT(FUNC_FLOOR)
    BIND_ENUM_CONSTANT(FUNC_ROUND)
    BIND_ENUM_CONSTANT(FUNC_CEIL)
    BIND_ENUM_CONSTANT(FUNC_FRAC)
    BIND_ENUM_CONSTANT(FUNC_SATURATE)
    BIND_ENUM_CONSTANT(FUNC_NEGATE)
    BIND_ENUM_CONSTANT(FUNC_ACOSH)
    BIND_ENUM_CONSTANT(FUNC_ASINH)
    BIND_ENUM_CONSTANT(FUNC_ATANH)
    BIND_ENUM_CONSTANT(FUNC_DEGREES)
    BIND_ENUM_CONSTANT(FUNC_EXP2)
    BIND_ENUM_CONSTANT(FUNC_INVERSE_SQRT)
    BIND_ENUM_CONSTANT(FUNC_LOG2)
    BIND_ENUM_CONSTANT(FUNC_RADIANS)
    BIND_ENUM_CONSTANT(FUNC_RECIPROCAL)
    BIND_ENUM_CONSTANT(FUNC_ROUNDEVEN)
    BIND_ENUM_CONSTANT(FUNC_TRUNC)
    BIND_ENUM_CONSTANT(FUNC_ONEMINUS)
}

VisualShaderNodeScalarFunc::VisualShaderNodeScalarFunc() {
    func = FUNC_SIGN;
    set_input_port_default_value(0, 0.0);
}

////////////// Vector Func

se_string_view VisualShaderNodeVectorFunc::get_caption() const {
    return ("VectorFunc");
}

int VisualShaderNodeVectorFunc::get_input_port_count() const {
    return 1;
}
VisualShaderNodeVectorFunc::PortType VisualShaderNodeVectorFunc::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeVectorFunc::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeVectorFunc::get_output_port_count() const {
    return 1;
}
VisualShaderNodeVectorFunc::PortType VisualShaderNodeVectorFunc::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeVectorFunc::get_output_port_name(int p_port) const {
    return StringName(); //no output port means the editor will be used as port
}

String VisualShaderNodeVectorFunc::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    static const char *vec_func_id[FUNC_ONEMINUS + 1] = {
        "normalize($)",
        "max(min($,vec3(1.0)),vec3(0.0))",
        "-($)",
        "1.0/($)",
        "",
        "",
        "abs($)",
        "acos($)",
        "acosh($)",
        "asin($)",
        "asinh($)",
        "atan($)",
        "atanh($)",
        "ceil($)",
        "cos($)",
        "cosh($)",
        "degrees($)",
        "exp($)",
        "exp2($)",
        "floor($)",
        "fract($)",
        "inversesqrt($)",
        "log($)",
        "log2($)",
        "radians($)",
        "round($)",
        "roundEven($)",
        "sign($)",
        "sin($)",
        "sinh($)",
        "sqrt($)",
        "tan($)",
        "tanh($)",
        "trunc($)",
        "vec3(1.0, 1.0, 1.0)-$"
    };

    String code;

    if (func == FUNC_RGB2HSV) {
        code += ("\t{\n");
        code += "\t\tvec3 c = " + p_input_vars[0] + ";\n";
        code += ("\t\tvec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);\n");
        code += ("\t\tvec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));\n");
        code += ("\t\tvec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));\n");
        code += ("\t\tfloat d = q.x - min(q.w, q.y);\n");
        code += ("\t\tfloat e = 1.0e-10;\n");
        code += "\t\t" + p_output_vars[0] + "=vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);\n";
        code += ("\t}\n");
    } else if (func == FUNC_HSV2RGB) {
        code += ("\t{\n");
        code += "\t\tvec3 c = " + p_input_vars[0] + ";\n";
        code += ("\t\tvec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);\n");
        code += ("\t\tvec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n");
        code += "\t\t" + p_output_vars[0] + "=c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n";
        code += ("\t}\n");

    } else {
        code += "\t" + p_output_vars[0] + "=" + StringUtils::replace((vec_func_id[func]),("$"), p_input_vars[0]) + ";\n";
    }

    return code;
}

void VisualShaderNodeVectorFunc::set_function(Function p_func) {

    func = p_func;
    emit_changed();
}

VisualShaderNodeVectorFunc::Function VisualShaderNodeVectorFunc::get_function() const {

    return func;
}

Vector<StringName> VisualShaderNodeVectorFunc::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("function");
    return props;
}

void VisualShaderNodeVectorFunc::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_function", {"func"}), &VisualShaderNodeVectorFunc::set_function);
    MethodBinder::bind_method(D_METHOD("get_function"), &VisualShaderNodeVectorFunc::get_function);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "function", PropertyHint::Enum, "Normalize,Saturate,Negate,Reciprocal,RGB2HSV,HSV2RGB,Abs,ACos,ACosH,ASin,ASinH,ATan,ATanH,Ceil,Cos,CosH,Degrees,Exp,Exp2,Floor,Frac,InverseSqrt,Log,Log2,Radians,Round,RoundEven,Sign,Sin,SinH,Sqrt,Tan,TanH,Trunc,OneMinus"), "set_function", "get_function");

    BIND_ENUM_CONSTANT(FUNC_NORMALIZE)
    BIND_ENUM_CONSTANT(FUNC_SATURATE)
    BIND_ENUM_CONSTANT(FUNC_NEGATE)
    BIND_ENUM_CONSTANT(FUNC_RECIPROCAL)
    BIND_ENUM_CONSTANT(FUNC_RGB2HSV)
    BIND_ENUM_CONSTANT(FUNC_HSV2RGB)
    BIND_ENUM_CONSTANT(FUNC_ABS)
    BIND_ENUM_CONSTANT(FUNC_ACOS)
    BIND_ENUM_CONSTANT(FUNC_ACOSH)
    BIND_ENUM_CONSTANT(FUNC_ASIN)
    BIND_ENUM_CONSTANT(FUNC_ASINH)
    BIND_ENUM_CONSTANT(FUNC_ATAN)
    BIND_ENUM_CONSTANT(FUNC_ATANH)
    BIND_ENUM_CONSTANT(FUNC_CEIL)
    BIND_ENUM_CONSTANT(FUNC_COS)
    BIND_ENUM_CONSTANT(FUNC_COSH)
    BIND_ENUM_CONSTANT(FUNC_DEGREES)
    BIND_ENUM_CONSTANT(FUNC_EXP)
    BIND_ENUM_CONSTANT(FUNC_EXP2)
    BIND_ENUM_CONSTANT(FUNC_FLOOR)
    BIND_ENUM_CONSTANT(FUNC_FRAC)
    BIND_ENUM_CONSTANT(FUNC_INVERSE_SQRT)
    BIND_ENUM_CONSTANT(FUNC_LOG)
    BIND_ENUM_CONSTANT(FUNC_LOG2)
    BIND_ENUM_CONSTANT(FUNC_RADIANS)
    BIND_ENUM_CONSTANT(FUNC_ROUND)
    BIND_ENUM_CONSTANT(FUNC_ROUNDEVEN)
    BIND_ENUM_CONSTANT(FUNC_SIGN)
    BIND_ENUM_CONSTANT(FUNC_SIN)
    BIND_ENUM_CONSTANT(FUNC_SINH)
    BIND_ENUM_CONSTANT(FUNC_SQRT)
    BIND_ENUM_CONSTANT(FUNC_TAN)
    BIND_ENUM_CONSTANT(FUNC_TANH)
    BIND_ENUM_CONSTANT(FUNC_TRUNC)
    BIND_ENUM_CONSTANT(FUNC_ONEMINUS)
}

VisualShaderNodeVectorFunc::VisualShaderNodeVectorFunc() {
    func = FUNC_NORMALIZE;
    set_input_port_default_value(0, Vector3());
}

////////////// ColorFunc

se_string_view VisualShaderNodeColorFunc::get_caption() const {
    return ("ColorFunc");
}

int VisualShaderNodeColorFunc::get_input_port_count() const {
    return 1;
}

VisualShaderNodeColorFunc::PortType VisualShaderNodeColorFunc::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeColorFunc::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeColorFunc::get_output_port_count() const {
    return 1;
}

VisualShaderNodeColorFunc::PortType VisualShaderNodeColorFunc::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeColorFunc::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeColorFunc::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    String code;

    switch (func) {
        case FUNC_GRAYSCALE:
            code += ("\t{\n");
            code += "\t\tvec3 c = " + p_input_vars[0] + ";\n";
            code += ("\t\tfloat max1 = max(c.r, c.g);\n");
            code += ("\t\tfloat max2 = max(max1, c.b);\n");
            code += ("\t\tfloat max3 = max(max1, max2);\n");
            code += "\t\t" + p_output_vars[0] + " = vec3(max3, max3, max3);\n";
            code += ("\t}\n");
            break;
        case FUNC_SEPIA:
            code += ("\t{\n");
            code += "\t\tvec3 c = " + p_input_vars[0] + ";\n";
            code += ("\t\tfloat r = (c.r * .393) + (c.g *.769) + (c.b * .189);\n");
            code += ("\t\tfloat g = (c.r * .349) + (c.g *.686) + (c.b * .168);\n");
            code += ("\t\tfloat b = (c.r * .272) + (c.g *.534) + (c.b * .131);\n");
            code += "\t\t" + p_output_vars[0] + " = vec3(r, g, b);\n";
            code += ("\t}\n");
            break;
    }

    return code;
}

void VisualShaderNodeColorFunc::set_function(Function p_func) {

    func = p_func;
    emit_changed();
}

VisualShaderNodeColorFunc::Function VisualShaderNodeColorFunc::get_function() const {

    return func;
}

Vector<StringName> VisualShaderNodeColorFunc::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("function");
    return props;
}

void VisualShaderNodeColorFunc::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_function", {"func"}), &VisualShaderNodeColorFunc::set_function);
    MethodBinder::bind_method(D_METHOD("get_function"), &VisualShaderNodeColorFunc::get_function);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "function", PropertyHint::Enum, "Grayscale,Sepia"), "set_function", "get_function");

    BIND_ENUM_CONSTANT(FUNC_GRAYSCALE)
    BIND_ENUM_CONSTANT(FUNC_SEPIA)
}

VisualShaderNodeColorFunc::VisualShaderNodeColorFunc() {
    func = FUNC_GRAYSCALE;
    set_input_port_default_value(0, Vector3());
}

////////////// Transform Func

se_string_view VisualShaderNodeTransformFunc::get_caption() const {
    return ("TransformFunc");
}

int VisualShaderNodeTransformFunc::get_input_port_count() const {
    return 1;
}

VisualShaderNodeTransformFunc::PortType VisualShaderNodeTransformFunc::get_input_port_type(int p_port) const {
    return PORT_TYPE_TRANSFORM;
}

StringName VisualShaderNodeTransformFunc::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeTransformFunc::get_output_port_count() const {
    return 1;
}

VisualShaderNodeTransformFunc::PortType VisualShaderNodeTransformFunc::get_output_port_type(int p_port) const {
    return PORT_TYPE_TRANSFORM;
}

StringName VisualShaderNodeTransformFunc::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeTransformFunc::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    static const char *funcs[FUNC_TRANSPOSE + 1] = {
        "inverse($)",
        "transpose($)"
    };

    String code;
    code += "\t" + p_output_vars[0] + "=" + StringUtils::replace((funcs[func]),("$"), p_input_vars[0]) + ";\n";
    return code;
}

void VisualShaderNodeTransformFunc::set_function(Function p_func) {

    func = p_func;
    emit_changed();
}

VisualShaderNodeTransformFunc::Function VisualShaderNodeTransformFunc::get_function() const {

    return func;
}

Vector<StringName> VisualShaderNodeTransformFunc::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("function");
    return props;
}

void VisualShaderNodeTransformFunc::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_function", {"func"}), &VisualShaderNodeTransformFunc::set_function);
    MethodBinder::bind_method(D_METHOD("get_function"), &VisualShaderNodeTransformFunc::get_function);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "function", PropertyHint::Enum, "Inverse,Transpose"), "set_function", "get_function");

    BIND_ENUM_CONSTANT(FUNC_INVERSE)
    BIND_ENUM_CONSTANT(FUNC_TRANSPOSE)
}

VisualShaderNodeTransformFunc::VisualShaderNodeTransformFunc() {
    func = FUNC_INVERSE;
    set_input_port_default_value(0, Transform());
}

////////////// Dot Product

se_string_view VisualShaderNodeDotProduct::get_caption() const {
    return ("DotProduct");
}

int VisualShaderNodeDotProduct::get_input_port_count() const {
    return 2;
}
VisualShaderNodeDotProduct::PortType VisualShaderNodeDotProduct::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeDotProduct::get_input_port_name(int p_port) const {
    return StringName(p_port == 0 ? "a" : "b");
}

int VisualShaderNodeDotProduct::get_output_port_count() const {
    return 1;
}
VisualShaderNodeDotProduct::PortType VisualShaderNodeDotProduct::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeDotProduct::get_output_port_name(int p_port) const {
    return StringName("dot");
}

String VisualShaderNodeDotProduct::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = dot( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n";
}

VisualShaderNodeDotProduct::VisualShaderNodeDotProduct() {
    set_input_port_default_value(0, Vector3());
    set_input_port_default_value(1, Vector3());
}

////////////// Vector Len

se_string_view VisualShaderNodeVectorLen::get_caption() const {
    return ("VectorLen");
}

int VisualShaderNodeVectorLen::get_input_port_count() const {
    return 1;
}
VisualShaderNodeVectorLen::PortType VisualShaderNodeVectorLen::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeVectorLen::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeVectorLen::get_output_port_count() const {
    return 1;
}
VisualShaderNodeVectorLen::PortType VisualShaderNodeVectorLen::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeVectorLen::get_output_port_name(int p_port) const {
    return ("length");
}

String VisualShaderNodeVectorLen::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = length( " + p_input_vars[0] + " );\n";
}

VisualShaderNodeVectorLen::VisualShaderNodeVectorLen() {
    set_input_port_default_value(0, Vector3());
}

////////////// Determinant

se_string_view VisualShaderNodeDeterminant::get_caption() const {
    return ("Determinant");
}

int VisualShaderNodeDeterminant::get_input_port_count() const {
    return 1;
}

VisualShaderNodeDeterminant::PortType VisualShaderNodeDeterminant::get_input_port_type(int p_port) const {
    return PORT_TYPE_TRANSFORM;
}

StringName VisualShaderNodeDeterminant::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeDeterminant::get_output_port_count() const {
    return 1;
}

VisualShaderNodeDeterminant::PortType VisualShaderNodeDeterminant::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeDeterminant::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeDeterminant::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = determinant( " + p_input_vars[0] + " );\n";
}

VisualShaderNodeDeterminant::VisualShaderNodeDeterminant() {
    set_input_port_default_value(0, Transform());
}

////////////// Scalar Derivative Function

se_string_view VisualShaderNodeScalarDerivativeFunc::get_caption() const {
    return ("ScalarDerivativeFunc");
}

int VisualShaderNodeScalarDerivativeFunc::get_input_port_count() const {
    return 1;
}

VisualShaderNodeScalarDerivativeFunc::PortType VisualShaderNodeScalarDerivativeFunc::get_input_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeScalarDerivativeFunc::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeScalarDerivativeFunc::get_output_port_count() const {
    return 1;
}

VisualShaderNodeScalarDerivativeFunc::PortType VisualShaderNodeScalarDerivativeFunc::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeScalarDerivativeFunc::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeScalarDerivativeFunc::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    static const char *funcs[FUNC_Y + 1] = {
        "fwidth($)",
        "dFdx($)",
        "dFdy($)"
    };

    String code;
    code += "\t" + p_output_vars[0] + "=" + StringUtils::replace((funcs[func]),("$"), p_input_vars[0]) + ";\n";
    return code;
}

void VisualShaderNodeScalarDerivativeFunc::set_function(Function p_func) {

    func = p_func;
    emit_changed();
}

VisualShaderNodeScalarDerivativeFunc::Function VisualShaderNodeScalarDerivativeFunc::get_function() const {

    return func;
}

Vector<StringName> VisualShaderNodeScalarDerivativeFunc::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("function");
    return props;
}

void VisualShaderNodeScalarDerivativeFunc::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_function", {"func"}), &VisualShaderNodeScalarDerivativeFunc::set_function);
    MethodBinder::bind_method(D_METHOD("get_function"), &VisualShaderNodeScalarDerivativeFunc::get_function);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "function", PropertyHint::Enum, "Sum,X,Y"), "set_function", "get_function");

    BIND_ENUM_CONSTANT(FUNC_SUM)
    BIND_ENUM_CONSTANT(FUNC_X)
    BIND_ENUM_CONSTANT(FUNC_Y)
}

VisualShaderNodeScalarDerivativeFunc::VisualShaderNodeScalarDerivativeFunc() {
    func = FUNC_SUM;
    set_input_port_default_value(0, 0.0);
}

////////////// Vector Derivative Function

se_string_view VisualShaderNodeVectorDerivativeFunc::get_caption() const {
    return ("VectorDerivativeFunc");
}

int VisualShaderNodeVectorDerivativeFunc::get_input_port_count() const {
    return 1;
}

VisualShaderNodeVectorDerivativeFunc::PortType VisualShaderNodeVectorDerivativeFunc::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorDerivativeFunc::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeVectorDerivativeFunc::get_output_port_count() const {
    return 1;
}

VisualShaderNodeVectorDerivativeFunc::PortType VisualShaderNodeVectorDerivativeFunc::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorDerivativeFunc::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeVectorDerivativeFunc::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    static const char *funcs[FUNC_Y + 1] = {
        "fwidth($)",
        "dFdx($)",
        "dFdy($)"
    };

    String code;
    code += "\t" + p_output_vars[0] + "=" + StringUtils::replace((funcs[func]),("$"), p_input_vars[0]) + ";\n";
    return code;
}

void VisualShaderNodeVectorDerivativeFunc::set_function(Function p_func) {

    func = p_func;
    emit_changed();
}

VisualShaderNodeVectorDerivativeFunc::Function VisualShaderNodeVectorDerivativeFunc::get_function() const {

    return func;
}

Vector<StringName> VisualShaderNodeVectorDerivativeFunc::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("function");
    return props;
}

void VisualShaderNodeVectorDerivativeFunc::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_function", {"func"}), &VisualShaderNodeVectorDerivativeFunc::set_function);
    MethodBinder::bind_method(D_METHOD("get_function"), &VisualShaderNodeVectorDerivativeFunc::get_function);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "function", PropertyHint::Enum, "Sum,X,Y"), "set_function", "get_function");

    BIND_ENUM_CONSTANT(FUNC_SUM)
    BIND_ENUM_CONSTANT(FUNC_X)
    BIND_ENUM_CONSTANT(FUNC_Y)
}

VisualShaderNodeVectorDerivativeFunc::VisualShaderNodeVectorDerivativeFunc() {
    func = FUNC_SUM;
    set_input_port_default_value(0, Vector3());
}

////////////// Scalar Clamp

se_string_view VisualShaderNodeScalarClamp::get_caption() const {
    return "ScalarClamp";
}

int VisualShaderNodeScalarClamp::get_input_port_count() const {
    return 3;
}

VisualShaderNodeScalarClamp::PortType VisualShaderNodeScalarClamp::get_input_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeScalarClamp::get_input_port_name(int p_port) const {
    if (p_port == 0)
        return StringName();
    else if (p_port == 1)
        return ("min");
    else if (p_port == 2)
        return ("max");
    return StringName();
}

int VisualShaderNodeScalarClamp::get_output_port_count() const {
    return 1;
}

VisualShaderNodeScalarClamp::PortType VisualShaderNodeScalarClamp::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeScalarClamp::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeScalarClamp::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = clamp( " + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + " );\n";
}

VisualShaderNodeScalarClamp::VisualShaderNodeScalarClamp() {
    set_input_port_default_value(0, 0.0);
    set_input_port_default_value(1, 0.0);
    set_input_port_default_value(2, 1.0);
}

////////////// Vector Clamp

se_string_view VisualShaderNodeVectorClamp::get_caption() const {
    return ("VectorClamp");
}

int VisualShaderNodeVectorClamp::get_input_port_count() const {
    return 3;
}

VisualShaderNodeVectorClamp::PortType VisualShaderNodeVectorClamp::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorClamp::get_input_port_name(int p_port) const {
    if (p_port == 0)
        return StringName();
    else if (p_port == 1)
        return StringName("min");
    else if (p_port == 2)
        return StringName("max");
    return StringName();
}

int VisualShaderNodeVectorClamp::get_output_port_count() const {
    return 1;
}

VisualShaderNodeVectorClamp::PortType VisualShaderNodeVectorClamp::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorClamp::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeVectorClamp::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = clamp( " + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + " );\n";
}

VisualShaderNodeVectorClamp::VisualShaderNodeVectorClamp() {
    set_input_port_default_value(0, Vector3(0, 0, 0));
    set_input_port_default_value(1, Vector3(0, 0, 0));
    set_input_port_default_value(2, Vector3(1, 1, 1));
}

////////////// FaceForward

se_string_view VisualShaderNodeFaceForward::get_caption() const {
    return ("FaceForward");
}

int VisualShaderNodeFaceForward::get_input_port_count() const {
    return 3;
}

VisualShaderNodeFaceForward::PortType VisualShaderNodeFaceForward::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeFaceForward::get_input_port_name(int p_port) const {
    switch (p_port) {
        case 0:
            return StringName("N");
        case 1:
            return StringName("I");
        case 2:
            return StringName("Nref");
        default:
            return StringName();
    }
}

int VisualShaderNodeFaceForward::get_output_port_count() const {
    return 1;
}

VisualShaderNodeFaceForward::PortType VisualShaderNodeFaceForward::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeFaceForward::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeFaceForward::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = faceforward( " + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + " );\n";
}

VisualShaderNodeFaceForward::VisualShaderNodeFaceForward() {
    set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0));
    set_input_port_default_value(1, Vector3(0.0, 0.0, 0.0));
    set_input_port_default_value(2, Vector3(0.0, 0.0, 0.0));
}

////////////// Outer Product

se_string_view VisualShaderNodeOuterProduct::get_caption() const {
    return ("OuterProduct");
}

int VisualShaderNodeOuterProduct::get_input_port_count() const {
    return 2;
}

VisualShaderNodeOuterProduct::PortType VisualShaderNodeOuterProduct::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeOuterProduct::get_input_port_name(int p_port) const {
    switch (p_port) {
        case 0:
            return StringName("c");
        case 1:
            return StringName("r");
        default:
            return StringName();
    }
}

int VisualShaderNodeOuterProduct::get_output_port_count() const {
    return 1;
}

VisualShaderNodeOuterProduct::PortType VisualShaderNodeOuterProduct::get_output_port_type(int p_port) const {
    return PORT_TYPE_TRANSFORM;
}

StringName VisualShaderNodeOuterProduct::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeOuterProduct::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = outerProduct( vec4(" + p_input_vars[0] + ", 0.0), vec4(" + p_input_vars[1] + ", 0.0) );\n";
}

VisualShaderNodeOuterProduct::VisualShaderNodeOuterProduct() {
    set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0));
    set_input_port_default_value(1, Vector3(0.0, 0.0, 0.0));
}

////////////// Vector-Scalar Step

se_string_view VisualShaderNodeVectorScalarStep::get_caption() const {
    return ("VectorScalarStep");
}

int VisualShaderNodeVectorScalarStep::get_input_port_count() const {
    return 2;
}

VisualShaderNodeVectorScalarStep::PortType VisualShaderNodeVectorScalarStep::get_input_port_type(int p_port) const {
    if (p_port == 0) {
        return PORT_TYPE_SCALAR;
    }
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorScalarStep::get_input_port_name(int p_port) const {
    if (p_port == 0)
        return StringName("edge");
    else if (p_port == 1)
        return StringName("x");
    return StringName();
}

int VisualShaderNodeVectorScalarStep::get_output_port_count() const {
    return 1;
}

VisualShaderNodeVectorScalarStep::PortType VisualShaderNodeVectorScalarStep::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorScalarStep::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeVectorScalarStep::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = step( " + p_input_vars[0] + ", " + p_input_vars[1] + " );\n";
}

VisualShaderNodeVectorScalarStep::VisualShaderNodeVectorScalarStep() {
    set_input_port_default_value(0, 0.0);
    set_input_port_default_value(1, Vector3(0.0, 0.0, 0.0));
}

////////////// Scalar SmoothStep

se_string_view VisualShaderNodeScalarSmoothStep::get_caption() const {
    return ("ScalarSmoothStep");
}

int VisualShaderNodeScalarSmoothStep::get_input_port_count() const {
    return 3;
}

VisualShaderNodeScalarSmoothStep::PortType VisualShaderNodeScalarSmoothStep::get_input_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeScalarSmoothStep::get_input_port_name(int p_port) const {
    if (p_port == 0)
        return StringName("edge0");
    else if (p_port == 1)
        return StringName("edge1");
    else if (p_port == 2)
        return StringName("x");
    return StringName();
}

int VisualShaderNodeScalarSmoothStep::get_output_port_count() const {
    return 1;
}

VisualShaderNodeScalarSmoothStep::PortType VisualShaderNodeScalarSmoothStep::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeScalarSmoothStep::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeScalarSmoothStep::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = smoothstep( " + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + " );\n";
}

VisualShaderNodeScalarSmoothStep::VisualShaderNodeScalarSmoothStep() {
    set_input_port_default_value(0, 0.0);
    set_input_port_default_value(1, 0.0);
    set_input_port_default_value(2, 0.0);
}

////////////// Vector SmoothStep

se_string_view VisualShaderNodeVectorSmoothStep::get_caption() const {
    return ("VectorSmoothStep");
}

int VisualShaderNodeVectorSmoothStep::get_input_port_count() const {
    return 3;
}

VisualShaderNodeVectorSmoothStep::PortType VisualShaderNodeVectorSmoothStep::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorSmoothStep::get_input_port_name(int p_port) const {
    if (p_port == 0)
        return StringName("edge0");
    else if (p_port == 1)
        return StringName("edge1");
    else if (p_port == 2)
        return StringName("x");
    return StringName();
}

int VisualShaderNodeVectorSmoothStep::get_output_port_count() const {
    return 1;
}

VisualShaderNodeVectorSmoothStep::PortType VisualShaderNodeVectorSmoothStep::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorSmoothStep::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeVectorSmoothStep::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = smoothstep( " + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + " );\n";
}

VisualShaderNodeVectorSmoothStep::VisualShaderNodeVectorSmoothStep() {
    set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0));
    set_input_port_default_value(1, Vector3(0.0, 0.0, 0.0));
    set_input_port_default_value(2, Vector3(0.0, 0.0, 0.0));
}

////////////// Vector-Scalar SmoothStep

se_string_view VisualShaderNodeVectorScalarSmoothStep::get_caption() const {
    return "VectorScalarSmoothStep";
}

int VisualShaderNodeVectorScalarSmoothStep::get_input_port_count() const {
    return 3;
}

VisualShaderNodeVectorScalarSmoothStep::PortType VisualShaderNodeVectorScalarSmoothStep::get_input_port_type(int p_port) const {
    if (p_port == 0) {
        return PORT_TYPE_SCALAR;
    } else if (p_port == 1) {
        return PORT_TYPE_SCALAR;
    }
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorScalarSmoothStep::get_input_port_name(int p_port) const {
    if (p_port == 0)
        return StringName("edge0");
    else if (p_port == 1)
        return StringName("edge1");
    else if (p_port == 2)
        return StringName("x");
    return StringName();
}

int VisualShaderNodeVectorScalarSmoothStep::get_output_port_count() const {
    return 1;
}

VisualShaderNodeVectorScalarSmoothStep::PortType VisualShaderNodeVectorScalarSmoothStep::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorScalarSmoothStep::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeVectorScalarSmoothStep::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = smoothstep( " + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + " );\n";
}

VisualShaderNodeVectorScalarSmoothStep::VisualShaderNodeVectorScalarSmoothStep() {
    set_input_port_default_value(0, 0.0);
    set_input_port_default_value(1, 0.0);
    set_input_port_default_value(2, Vector3(0.0, 0.0, 0.0));
}

////////////// Distance

se_string_view VisualShaderNodeVectorDistance::get_caption() const {
    return ("Distance");
}

int VisualShaderNodeVectorDistance::get_input_port_count() const {
    return 2;
}

VisualShaderNodeVectorDistance::PortType VisualShaderNodeVectorDistance::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorDistance::get_input_port_name(int p_port) const {
    if (p_port == 0) {
        return StringName("p0");
    } else if (p_port == 1) {
        return StringName("p1");
    }
    return StringName();
}

int VisualShaderNodeVectorDistance::get_output_port_count() const {
    return 1;
}

VisualShaderNodeVectorDistance::PortType VisualShaderNodeVectorDistance::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeVectorDistance::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeVectorDistance::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = distance( " + p_input_vars[0] + " , " + p_input_vars[1] + " );\n";
}

VisualShaderNodeVectorDistance::VisualShaderNodeVectorDistance() {
    set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0));
    set_input_port_default_value(1, Vector3(0.0, 0.0, 0.0));
}

////////////// Refract Vector

se_string_view VisualShaderNodeVectorRefract::get_caption() const {
    return ("Refract");
}

int VisualShaderNodeVectorRefract::get_input_port_count() const {
    return 3;
}

VisualShaderNodeVectorRefract::PortType VisualShaderNodeVectorRefract::get_input_port_type(int p_port) const {

    if (p_port == 2) {
        return PORT_TYPE_SCALAR;
    }

    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorRefract::get_input_port_name(int p_port) const {
    if (p_port == 0) {
        return StringName("I");
    } else if (p_port == 1) {
        return StringName("N");
    } else if (p_port == 2) {
        return StringName("eta");
    }
    return StringName();
}

int VisualShaderNodeVectorRefract::get_output_port_count() const {
    return 1;
}

VisualShaderNodeVectorRefract::PortType VisualShaderNodeVectorRefract::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorRefract::get_output_port_name(int p_port) const {
    return StringName();
}

String VisualShaderNodeVectorRefract::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = refract( " + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + " );\n";
}

VisualShaderNodeVectorRefract::VisualShaderNodeVectorRefract() {
    set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0));
    set_input_port_default_value(1, Vector3(0.0, 0.0, 0.0));
    set_input_port_default_value(2, 0.0);
}

////////////// Scalar Mix

se_string_view VisualShaderNodeScalarInterp::get_caption() const {
    return ("ScalarMix");
}

int VisualShaderNodeScalarInterp::get_input_port_count() const {
    return 3;
}

VisualShaderNodeScalarInterp::PortType VisualShaderNodeScalarInterp::get_input_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeScalarInterp::get_input_port_name(int p_port) const {
    if (p_port == 0) {
        return StringName("a");
    } else if (p_port == 1) {
        return StringName("b");
    } else {
        return StringName("weight");
    }
}

int VisualShaderNodeScalarInterp::get_output_port_count() const {
    return 1;
}

VisualShaderNodeScalarInterp::PortType VisualShaderNodeScalarInterp::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeScalarInterp::get_output_port_name(int p_port) const {
    return StringName("mix");
}

String VisualShaderNodeScalarInterp::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = mix( " + p_input_vars[0] + " , " + p_input_vars[1] + " , " + p_input_vars[2] + " );\n";
}

VisualShaderNodeScalarInterp::VisualShaderNodeScalarInterp() {
    set_input_port_default_value(0, 0.0);
    set_input_port_default_value(1, 1.0);
    set_input_port_default_value(2, 0.5);
}

////////////// Vector Mix

se_string_view VisualShaderNodeVectorInterp::get_caption() const {
    return ("VectorMix");
}

int VisualShaderNodeVectorInterp::get_input_port_count() const {
    return 3;
}

VisualShaderNodeVectorInterp::PortType VisualShaderNodeVectorInterp::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorInterp::get_input_port_name(int p_port) const {
    if (p_port == 0) {
        return StringName("a");
    } else if (p_port == 1) {
        return StringName("b");
    } else {
        return StringName("weight");
    }
}

int VisualShaderNodeVectorInterp::get_output_port_count() const {
    return 1;
}

VisualShaderNodeVectorInterp::PortType VisualShaderNodeVectorInterp::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorInterp::get_output_port_name(int p_port) const {
    return StringName("mix");
}

String VisualShaderNodeVectorInterp::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = mix( " + p_input_vars[0] + " , " + p_input_vars[1] + " , " + p_input_vars[2] + " );\n";
}

VisualShaderNodeVectorInterp::VisualShaderNodeVectorInterp() {
    set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0));
    set_input_port_default_value(1, Vector3(1.0, 1.0, 1.0));
    set_input_port_default_value(2, Vector3(0.5, 0.5, 0.5));
}

////////////// Vector Mix (by scalar)

se_string_view VisualShaderNodeVectorScalarMix::get_caption() const {
    return ("VectorScalarMix");
}

int VisualShaderNodeVectorScalarMix::get_input_port_count() const {
    return 3;
}

VisualShaderNodeVectorScalarMix::PortType VisualShaderNodeVectorScalarMix::get_input_port_type(int p_port) const {
    if (p_port == 2)
        return PORT_TYPE_SCALAR;
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorScalarMix::get_input_port_name(int p_port) const {
    if (p_port == 0) {
        return StringName("a");
    } else if (p_port == 1) {
        return StringName("b");
    } else {
        return StringName("weight");
    }
}

int VisualShaderNodeVectorScalarMix::get_output_port_count() const {
    return 1;
}

VisualShaderNodeVectorScalarMix::PortType VisualShaderNodeVectorScalarMix::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeVectorScalarMix::get_output_port_name(int p_port) const {
    return StringName("mix");
}

String VisualShaderNodeVectorScalarMix::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = mix( " + p_input_vars[0] + " , " + p_input_vars[1] + " , " + p_input_vars[2] + " );\n";
}

VisualShaderNodeVectorScalarMix::VisualShaderNodeVectorScalarMix() {
    set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0));
    set_input_port_default_value(1, Vector3(1.0, 1.0, 1.0));
    set_input_port_default_value(2, 0.5);
}

////////////// Vector Compose
se_string_view VisualShaderNodeVectorCompose::get_caption() const {
    return ("VectorCompose");
}

int VisualShaderNodeVectorCompose::get_input_port_count() const {
    return 3;
}
VisualShaderNodeVectorCompose::PortType VisualShaderNodeVectorCompose::get_input_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeVectorCompose::get_input_port_name(int p_port) const {
    if (p_port == 0) {
        return StringName("x");
    } else if (p_port == 1) {
        return StringName("y");
    } else {
        return StringName("z");
    }
}

int VisualShaderNodeVectorCompose::get_output_port_count() const {
    return 1;
}
VisualShaderNodeVectorCompose::PortType VisualShaderNodeVectorCompose::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeVectorCompose::get_output_port_name(int p_port) const {
    return StringName("vec");
}

String VisualShaderNodeVectorCompose::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = vec3( " + p_input_vars[0] + " , " + p_input_vars[1] + " , " + p_input_vars[2] + " );\n";
}

VisualShaderNodeVectorCompose::VisualShaderNodeVectorCompose() {

    set_input_port_default_value(0, 0.0);
    set_input_port_default_value(1, 0.0);
    set_input_port_default_value(2, 0.0);
}

////////////// Transform Compose

se_string_view VisualShaderNodeTransformCompose::get_caption() const {
    return ("TransformCompose");
}

int VisualShaderNodeTransformCompose::get_input_port_count() const {
    return 4;
}
VisualShaderNodeTransformCompose::PortType VisualShaderNodeTransformCompose::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeTransformCompose::get_input_port_name(int p_port) const {
    if (p_port == 0) {
        return StringName("x");
    } else if (p_port == 1) {
        return StringName("y");
    } else if (p_port == 2) {
        return StringName("z");
    } else {
        return StringName("origin");
    }
}

int VisualShaderNodeTransformCompose::get_output_port_count() const {
    return 1;
}
VisualShaderNodeTransformCompose::PortType VisualShaderNodeTransformCompose::get_output_port_type(int p_port) const {
    return PORT_TYPE_TRANSFORM;
}
StringName VisualShaderNodeTransformCompose::get_output_port_name(int p_port) const {
    return StringName("xform");
}

String VisualShaderNodeTransformCompose::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = mat4( vec4(" + p_input_vars[0] + ", 0.0) , vec4(" + p_input_vars[1] + ", 0.0) , vec4(" + p_input_vars[2] + ",0.0), vec4(" + p_input_vars[3] + ",1.0) );\n";
}

VisualShaderNodeTransformCompose::VisualShaderNodeTransformCompose() {

    set_input_port_default_value(0, Vector3());
    set_input_port_default_value(1, Vector3());
    set_input_port_default_value(2, Vector3());
    set_input_port_default_value(3, Vector3());
}

////////////// Vector Decompose
se_string_view VisualShaderNodeVectorDecompose::get_caption() const {
    return ("VectorDecompose");
}

int VisualShaderNodeVectorDecompose::get_input_port_count() const {
    return 1;
}
VisualShaderNodeVectorDecompose::PortType VisualShaderNodeVectorDecompose::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeVectorDecompose::get_input_port_name(int p_port) const {
    return StringName("vec");
}

int VisualShaderNodeVectorDecompose::get_output_port_count() const {
    return 3;
}
VisualShaderNodeVectorDecompose::PortType VisualShaderNodeVectorDecompose::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeVectorDecompose::get_output_port_name(int p_port) const {
    if (p_port == 0) {
        return StringName("x");
    } else if (p_port == 1) {
        return StringName("y");
    } else {
        return StringName("z");
    }
}

String VisualShaderNodeVectorDecompose::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    String code;
    code += "\t" + p_output_vars[0] + " = " + p_input_vars[0] + ".x;\n";
    code += "\t" + p_output_vars[1] + " = " + p_input_vars[0] + ".y;\n";
    code += "\t" + p_output_vars[2] + " = " + p_input_vars[0] + ".z;\n";
    return code;
}

VisualShaderNodeVectorDecompose::VisualShaderNodeVectorDecompose() {
    set_input_port_default_value(0, Vector3());
}

////////////// Transform Decompose

se_string_view VisualShaderNodeTransformDecompose::get_caption() const {
    return ("TransformDecompose");
}

int VisualShaderNodeTransformDecompose::get_input_port_count() const {
    return 1;
}
VisualShaderNodeTransformDecompose::PortType VisualShaderNodeTransformDecompose::get_input_port_type(int p_port) const {
    return PORT_TYPE_TRANSFORM;
}
StringName VisualShaderNodeTransformDecompose::get_input_port_name(int p_port) const {
    return StringName("xform");
}

int VisualShaderNodeTransformDecompose::get_output_port_count() const {
    return 4;
}
VisualShaderNodeTransformDecompose::PortType VisualShaderNodeTransformDecompose::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeTransformDecompose::get_output_port_name(int p_port) const {
    if (p_port == 0) {
        return StringName("x");
    } else if (p_port == 1) {
        return StringName("y");
    } else if (p_port == 2) {
        return StringName("z");
    } else {
        return StringName("origin");
    }
}

String VisualShaderNodeTransformDecompose::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    String code;
    code += "\t" + p_output_vars[0] + " = " + p_input_vars[0] + "[0].xyz;\n";
    code += "\t" + p_output_vars[1] + " = " + p_input_vars[0] + "[1].xyz;\n";
    code += "\t" + p_output_vars[2] + " = " + p_input_vars[0] + "[2].xyz;\n";
    code += "\t" + p_output_vars[3] + " = " + p_input_vars[0] + "[3].xyz;\n";
    return code;
}

VisualShaderNodeTransformDecompose::VisualShaderNodeTransformDecompose() {
    set_input_port_default_value(0, Transform());
}

////////////// Scalar Uniform

se_string_view VisualShaderNodeScalarUniform::get_caption() const {
    return ("ScalarUniform");
}

int VisualShaderNodeScalarUniform::get_input_port_count() const {
    return 0;
}
VisualShaderNodeScalarUniform::PortType VisualShaderNodeScalarUniform::get_input_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeScalarUniform::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeScalarUniform::get_output_port_count() const {
    return 1;
}
VisualShaderNodeScalarUniform::PortType VisualShaderNodeScalarUniform::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeScalarUniform::get_output_port_name(int p_port) const {
    return StringName(); //no output port means the editor will be used as port
}

String VisualShaderNodeScalarUniform::generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {
    return String("uniform float ") + get_uniform_name() + ";\n";
}
String VisualShaderNodeScalarUniform::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = " + get_uniform_name() + ";\n";
}

VisualShaderNodeScalarUniform::VisualShaderNodeScalarUniform() {
}

////////////// Boolean Uniform

se_string_view VisualShaderNodeBooleanUniform::get_caption() const {
    return ("BooleanUniform");
}

int VisualShaderNodeBooleanUniform::get_input_port_count() const {
    return 0;
}

VisualShaderNodeBooleanUniform::PortType VisualShaderNodeBooleanUniform::get_input_port_type(int p_port) const {
    return PORT_TYPE_BOOLEAN;
}

StringName VisualShaderNodeBooleanUniform::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeBooleanUniform::get_output_port_count() const {
    return 1;
}

VisualShaderNodeBooleanUniform::PortType VisualShaderNodeBooleanUniform::get_output_port_type(int p_port) const {
    return PORT_TYPE_BOOLEAN;
}

StringName VisualShaderNodeBooleanUniform::get_output_port_name(int p_port) const {
    return StringName(); //no output port means the editor will be used as port
}

String VisualShaderNodeBooleanUniform::generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {
    return String("uniform bool ") + get_uniform_name() + ";\n";
}

String VisualShaderNodeBooleanUniform::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = " + get_uniform_name() + ";\n";
}

VisualShaderNodeBooleanUniform::VisualShaderNodeBooleanUniform() {
}

////////////// Color Uniform

se_string_view VisualShaderNodeColorUniform::get_caption() const {
    return ("ColorUniform");
}

int VisualShaderNodeColorUniform::get_input_port_count() const {
    return 0;
}
VisualShaderNodeColorUniform::PortType VisualShaderNodeColorUniform::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeColorUniform::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeColorUniform::get_output_port_count() const {
    return 2;
}
VisualShaderNodeColorUniform::PortType VisualShaderNodeColorUniform::get_output_port_type(int p_port) const {
    return p_port == 0 ? PORT_TYPE_VECTOR : PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeColorUniform::get_output_port_name(int p_port) const {
    return StringName(p_port == 0 ? "color" : "alpha"); //no output port means the editor will be used as port
}

String VisualShaderNodeColorUniform::generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {

    return String("uniform vec4 ") + get_uniform_name() + " : hint_color;\n";
}

String VisualShaderNodeColorUniform::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    String code = "\t" + p_output_vars[0] + " = " + get_uniform_name() + ".rgb;\n";
    code += "\t" + p_output_vars[1] + " = " + get_uniform_name() + ".a;\n";
    return code;
}

VisualShaderNodeColorUniform::VisualShaderNodeColorUniform() {
}

////////////// Vector Uniform

se_string_view VisualShaderNodeVec3Uniform::get_caption() const {
    return ("VectorUniform");
}

int VisualShaderNodeVec3Uniform::get_input_port_count() const {
    return 0;
}
VisualShaderNodeVec3Uniform::PortType VisualShaderNodeVec3Uniform::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeVec3Uniform::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeVec3Uniform::get_output_port_count() const {
    return 1;
}
VisualShaderNodeVec3Uniform::PortType VisualShaderNodeVec3Uniform::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeVec3Uniform::get_output_port_name(int p_port) const {
    return StringName(); //no output port means the editor will be used as port
}
String VisualShaderNodeVec3Uniform::generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {
    return String("uniform vec3 ") + get_uniform_name() + ";\n";
}

String VisualShaderNodeVec3Uniform::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = " + get_uniform_name() + ";\n";
}

VisualShaderNodeVec3Uniform::VisualShaderNodeVec3Uniform() {
}

////////////// Transform Uniform

se_string_view VisualShaderNodeTransformUniform::get_caption() const {
    return ("TransformUniform");
}

int VisualShaderNodeTransformUniform::get_input_port_count() const {
    return 0;
}
VisualShaderNodeTransformUniform::PortType VisualShaderNodeTransformUniform::get_input_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}
StringName VisualShaderNodeTransformUniform::get_input_port_name(int p_port) const {
    return StringName();
}

int VisualShaderNodeTransformUniform::get_output_port_count() const {
    return 1;
}
VisualShaderNodeTransformUniform::PortType VisualShaderNodeTransformUniform::get_output_port_type(int p_port) const {
    return PORT_TYPE_TRANSFORM;
}
StringName VisualShaderNodeTransformUniform::get_output_port_name(int p_port) const {
    return StringName(); //no output port means the editor will be used as port
}
String VisualShaderNodeTransformUniform::generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {
    return String("uniform mat4 ") + get_uniform_name() + ";\n";
}

String VisualShaderNodeTransformUniform::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return "\t" + p_output_vars[0] + " = " + get_uniform_name() + ";\n";
}

VisualShaderNodeTransformUniform::VisualShaderNodeTransformUniform() {
}

////////////// Texture Uniform

se_string_view VisualShaderNodeTextureUniform::get_caption() const {
    return ("TextureUniform");
}

int VisualShaderNodeTextureUniform::get_input_port_count() const {
    return 2;
}
VisualShaderNodeTextureUniform::PortType VisualShaderNodeTextureUniform::get_input_port_type(int p_port) const {
    return p_port == 0 ? PORT_TYPE_VECTOR : PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeTextureUniform::get_input_port_name(int p_port) const {
    return StringName(p_port == 0 ? "uv" : "lod");
}

int VisualShaderNodeTextureUniform::get_output_port_count() const {
    return 3;
}
VisualShaderNodeTextureUniform::PortType VisualShaderNodeTextureUniform::get_output_port_type(int p_port) const {
    switch (p_port) {
        case 0:
            return PORT_TYPE_VECTOR;
        case 1:
            return PORT_TYPE_SCALAR;
        case 2:
            return PORT_TYPE_SAMPLER;
        default:
            return PORT_TYPE_SCALAR;
    }
}
StringName VisualShaderNodeTextureUniform::get_output_port_name(int p_port) const {
    switch (p_port) {
        case 0:
            return StringName("rgb");
        case 1:
            return StringName("alpha");
        case 2:
            return StringName("sampler2D");
        default:
            return StringName();
    }
}

String VisualShaderNodeTextureUniform::generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {
    String code("uniform sampler2D " + get_uniform_name());

    switch (texture_type) {
        case TYPE_DATA:
            if (color_default == COLOR_DEFAULT_BLACK)
                code += (" : hint_black;\n");
            else
                code += (";\n");
            break;
        case TYPE_COLOR:
            if (color_default == COLOR_DEFAULT_BLACK)
                code += (" : hint_black_albedo;\n");
            else
                code += (" : hint_albedo;\n");
            break;
        case TYPE_NORMALMAP: code += (" : hint_normal;\n"); break;
        case TYPE_ANISO: code += (" : hint_aniso;\n"); break;
    }

    return code;
}

String VisualShaderNodeTextureUniform::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    String id(get_uniform_name());
    String code("\t{\n");
    if (p_input_vars[0].empty()) { // Use UV by default.
        if (p_input_vars[1].empty()) {
            code += "\t\tvec4 n_tex_read = texture( " + id + " , UV.xy );\n";
        } else {
            code += "\t\tvec4 n_tex_read = textureLod( " + id + " , UV.xy , " + p_input_vars[1] + " );\n";
        }
    } else if (p_input_vars[1].empty()) {
        //no lod
        code += "\t\tvec4 n_tex_read = texture( " + id + " , " + p_input_vars[0] + ".xy );\n";
    } else {
        code += "\t\tvec4 n_tex_read = textureLod( " + id + " , " + p_input_vars[0] + ".xy , " + p_input_vars[1] + " );\n";
    }

    code += "\t\t" + p_output_vars[0] + " = n_tex_read.rgb;\n";
    code += "\t\t" + p_output_vars[1] + " = n_tex_read.a;\n";
    code += "\t}\n";
    return code;
}

void VisualShaderNodeTextureUniform::set_texture_type(TextureType p_type) {

    texture_type = p_type;
    emit_changed();
}

VisualShaderNodeTextureUniform::TextureType VisualShaderNodeTextureUniform::get_texture_type() const {
    return texture_type;
}

void VisualShaderNodeTextureUniform::set_color_default(ColorDefault p_default) {
    color_default = p_default;
    emit_changed();
}
VisualShaderNodeTextureUniform::ColorDefault VisualShaderNodeTextureUniform::get_color_default() const {
    return color_default;
}

Vector<StringName> VisualShaderNodeTextureUniform::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("texture_type");
    props.push_back("color_default");
    return props;
}

void VisualShaderNodeTextureUniform::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("set_texture_type", {"type"}), &VisualShaderNodeTextureUniform::set_texture_type);
    MethodBinder::bind_method(D_METHOD("get_texture_type"), &VisualShaderNodeTextureUniform::get_texture_type);

    MethodBinder::bind_method(D_METHOD("set_color_default", {"type"}), &VisualShaderNodeTextureUniform::set_color_default);
    MethodBinder::bind_method(D_METHOD("get_color_default"), &VisualShaderNodeTextureUniform::get_color_default);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "texture_type", PropertyHint::Enum, "Data,Color,Normalmap,Aniso"), "set_texture_type", "get_texture_type");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "color_default", PropertyHint::Enum, "White Default,Black Default"), "set_color_default", "get_color_default");

    BIND_ENUM_CONSTANT(TYPE_DATA)
    BIND_ENUM_CONSTANT(TYPE_COLOR)
    BIND_ENUM_CONSTANT(TYPE_NORMALMAP)
    BIND_ENUM_CONSTANT(TYPE_ANISO)

    BIND_ENUM_CONSTANT(COLOR_DEFAULT_WHITE)
    BIND_ENUM_CONSTANT(COLOR_DEFAULT_BLACK)
}
StringName VisualShaderNodeTextureUniform::get_input_port_default_hint(int p_port) const {
    if (p_port == 0) {
        return StringName("UV.xy");
    }
    return StringName();
}
VisualShaderNodeTextureUniform::VisualShaderNodeTextureUniform() {
    texture_type = TYPE_DATA;
    color_default = COLOR_DEFAULT_WHITE;
}

////////////// Texture Uniform (Triplanar)

se_string_view VisualShaderNodeTextureUniformTriplanar::get_caption() const {
    return ("TextureUniformTriplanar");
}

int VisualShaderNodeTextureUniformTriplanar::get_input_port_count() const {
    return 2;
}

VisualShaderNodeTextureUniform::PortType VisualShaderNodeTextureUniformTriplanar::get_input_port_type(int p_port) const {
    if (p_port == 0) {
        return PORT_TYPE_VECTOR;
    } else if (p_port == 1) {
        return PORT_TYPE_VECTOR;
    }
    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeTextureUniformTriplanar::get_input_port_name(int p_port) const {
    if (p_port == 0) {
        return StringName("weights");
    } else if (p_port == 1) {
        return StringName("pos");
    }
    return StringName();
}

String VisualShaderNodeTextureUniformTriplanar::generate_global_per_node(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {

    String code;

    code += "// TRIPLANAR FUNCTION GLOBAL CODE\n";
    code += "\tvec4 triplanar_texture(sampler2D p_sampler, vec3 p_weights, vec3 p_triplanar_pos) {\n";
    code += "\t\tvec4 samp = vec4(0.0);\n";
    code += "\t\tsamp += texture(p_sampler, p_triplanar_pos.xy) * p_weights.z;\n";
    code += "\t\tsamp += texture(p_sampler, p_triplanar_pos.xz) * p_weights.y;\n";
    code += "\t\tsamp += texture(p_sampler, p_triplanar_pos.zy * vec2(-1.0, 1.0)) * p_weights.x;\n";
    code += "\t\treturn samp;\n";
    code += "\t}\n";
    code += "\n";
    code += "\tuniform vec3 triplanar_scale = vec3(1.0, 1.0, 1.0);\n";
    code += "\tuniform vec3 triplanar_offset;\n";
    code += "\tuniform float triplanar_sharpness = 0.5;\n";
    code += "\n";
    code += "\tvarying vec3 triplanar_power_normal;\n";
    code += "\tvarying vec3 triplanar_pos;\n";

    return (code);
}

String VisualShaderNodeTextureUniformTriplanar::generate_global_per_func(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {

    String code;

    if (p_type == VisualShader::TYPE_VERTEX) {

        code += "\t// TRIPLANAR FUNCTION VERTEX CODE\n";
        code += "\t\ttriplanar_power_normal = pow(abs(NORMAL), vec3(triplanar_sharpness));\n";
        code += "\t\ttriplanar_power_normal /= dot(triplanar_power_normal, vec3(1.0));\n";
        code += "\t\ttriplanar_pos = VERTEX * triplanar_scale + triplanar_offset;\n";
        code += "\t\ttriplanar_pos *= vec3(1.0, -1.0, 1.0);\n";
    }

    return (code);
}

String VisualShaderNodeTextureUniformTriplanar::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    String id(get_uniform_name());
    String code("\t{\n");

    if (p_input_vars[0].empty() && p_input_vars[1].empty()) {
        code += "\t\tvec4 n_tex_read = triplanar_texture( " + id + ", triplanar_power_normal, triplanar_pos );\n";
    } else if (!p_input_vars[0].empty() && p_input_vars[1].empty()) {
        code += "\t\tvec4 n_tex_read = triplanar_texture( " + id + ", " + p_input_vars[0] + ", triplanar_pos );\n";
    } else if (p_input_vars[0].empty() && !p_input_vars[1].empty()) {
        code += "\t\tvec4 n_tex_read = triplanar_texture( " + id + ", triplanar_power_normal," + p_input_vars[1] + " );\n";
    } else {
        code += "\t\tvec4 n_tex_read = triplanar_texture( " + id + ", " + p_input_vars[0] + ", " + p_input_vars[1] + " );\n";
    }

    code += "\t\t" + p_output_vars[0] + " = n_tex_read.rgb;\n";
    code += "\t\t" + p_output_vars[1] + " = n_tex_read.a;\n";
    code += ("\t}\n");

    return code;
}

StringName VisualShaderNodeTextureUniformTriplanar::get_input_port_default_hint(int p_port) const {
    if (p_port == 0) {
        return StringName("default");
    } else if (p_port == 1) {
        return StringName("default");
    }
    return StringName();
}
VisualShaderNodeTextureUniformTriplanar::VisualShaderNodeTextureUniformTriplanar() {
}

////////////// CubeMap Uniform

se_string_view VisualShaderNodeCubeMapUniform::get_caption() const {
    return ("CubeMapUniform");
}
int VisualShaderNodeCubeMapUniform::get_output_port_count() const {
    return 1;
}

VisualShaderNodeCubeMapUniform::PortType VisualShaderNodeCubeMapUniform::get_output_port_type(int p_port) const {
    return PORT_TYPE_SAMPLER;
}

StringName VisualShaderNodeCubeMapUniform::get_output_port_name(int p_port) const {
    return StringName("samplerCube");
}

int VisualShaderNodeCubeMapUniform::get_input_port_count() const {
    return 0;
}
VisualShaderNodeCubeMapUniform::PortType VisualShaderNodeCubeMapUniform::get_input_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeCubeMapUniform::get_input_port_name(int p_port) const {
    return StringName();
}

StringName VisualShaderNodeCubeMapUniform::get_input_port_default_hint(int p_port) const {
    return StringName();
}

String VisualShaderNodeCubeMapUniform::generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {
    String code("uniform samplerCube " + get_uniform_name());

    switch (texture_type) {
        case TYPE_DATA:
            if (color_default == COLOR_DEFAULT_BLACK)
                code += (" : hint_black;\n");
            else
                code += (";\n");
            break;
        case TYPE_COLOR:
            if (color_default == COLOR_DEFAULT_BLACK)
                code += (" : hint_black_albedo;\n");
            else
                code += (" : hint_albedo;\n");
            break;
        case TYPE_NORMALMAP: code += (" : hint_normal;\n"); break;
        case TYPE_ANISO: code += (" : hint_aniso;\n"); break;
    }

    return code;
}

String VisualShaderNodeCubeMapUniform::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return String();
}

VisualShaderNodeCubeMapUniform::VisualShaderNodeCubeMapUniform() {
}

////////////// If

se_string_view VisualShaderNodeIf::get_caption() const {
    return ("If");
}

int VisualShaderNodeIf::get_input_port_count() const {
    return 6;
}

VisualShaderNodeIf::PortType VisualShaderNodeIf::get_input_port_type(int p_port) const {
    if (p_port == 0 || p_port == 1 || p_port == 2) {
        return PORT_TYPE_SCALAR;
    }
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeIf::get_input_port_name(int p_port) const {
    switch (p_port) {
        case 0:
            return StringName("a");
        case 1:
            return StringName("b");
        case 2:
            return StringName("tolerance");
        case 3:
            return StringName("a == b");
        case 4:
            return StringName("a > b");
        case 5:
            return StringName("a < b");
        default:
            return StringName();
    }
}

int VisualShaderNodeIf::get_output_port_count() const {
    return 1;
}

VisualShaderNodeIf::PortType VisualShaderNodeIf::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeIf::get_output_port_name(int p_port) const {
    return StringName("result");
}

String VisualShaderNodeIf::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    String code;
    code += "\tif(abs(" + p_input_vars[0] + "-" + p_input_vars[1] + ")<" + p_input_vars[2] + ")\n"; // abs(a - b) < tolerance eg. a == b
    code += ("\t{\n");
    code += "\t\t" + p_output_vars[0] + "=" + p_input_vars[3] + ";\n";
    code += ("\t}\n");
    code += "\telse if(" + p_input_vars[0] + "<" + p_input_vars[1] + ")\n"; // a < b
    code += ("\t{\n");
    code += "\t\t" + p_output_vars[0] + "=" + p_input_vars[5] + ";\n";
    code += ("\t}\n");
    code += ("\telse\n"); // a > b (or a >= b if abs(a - b) < tolerance is false)
    code += ("\t{\n");
    code += "\t\t" + p_output_vars[0] + "=" + p_input_vars[4] + ";\n";
    code += ("\t}\n");
    return code;
}

VisualShaderNodeIf::VisualShaderNodeIf() {
    set_input_port_default_value(0, 0.0);
    set_input_port_default_value(1, 0.0);
    set_input_port_default_value(2, CMP_EPSILON);
    set_input_port_default_value(3, Vector3(0.0, 0.0, 0.0));
    set_input_port_default_value(4, Vector3(0.0, 0.0, 0.0));
    set_input_port_default_value(5, Vector3(0.0, 0.0, 0.0));
}

////////////// Switch

se_string_view VisualShaderNodeSwitch::get_caption() const {
    return ("VectorSwitch");
}

int VisualShaderNodeSwitch::get_input_port_count() const {
    return 3;
}

VisualShaderNodeSwitch::PortType VisualShaderNodeSwitch::get_input_port_type(int p_port) const {
    if (p_port == 0) {
        return PORT_TYPE_BOOLEAN;
    }
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeSwitch::get_input_port_name(int p_port) const {
    switch (p_port) {
        case 0:
            return StringName("value");
        case 1:
            return StringName("true");
        case 2:
            return StringName("false");
        default:
            return StringName();
    }
}

int VisualShaderNodeSwitch::get_output_port_count() const {
    return 1;
}

VisualShaderNodeSwitch::PortType VisualShaderNodeSwitch::get_output_port_type(int p_port) const {
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeSwitch::get_output_port_name(int p_port) const {
    return StringName("result");
}

String VisualShaderNodeSwitch::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    String code;
    code += "\tif(" + p_input_vars[0] + ")\n";
    code += ("\t{\n");
    code += "\t\t" + p_output_vars[0] + "=" + p_input_vars[1] + ";\n";
    code += ("\t}\n");
    code += ("\telse\n");
    code += ("\t{\n");
    code += "\t\t" + p_output_vars[0] + "=" + p_input_vars[2] + ";\n";
    code += ("\t}\n");
    return code;
}

VisualShaderNodeSwitch::VisualShaderNodeSwitch() {
    set_input_port_default_value(0, false);
    set_input_port_default_value(1, Vector3(1.0, 1.0, 1.0));
    set_input_port_default_value(2, Vector3(0.0, 0.0, 0.0));
}

////////////// Switch(scalar)

se_string_view VisualShaderNodeScalarSwitch::get_caption() const {
    return ("ScalarSwitch");
}

VisualShaderNodeScalarSwitch::PortType VisualShaderNodeScalarSwitch::get_input_port_type(int p_port) const {
    if (p_port == 0) {
        return PORT_TYPE_BOOLEAN;
    }
    return PORT_TYPE_SCALAR;
}

VisualShaderNodeScalarSwitch::PortType VisualShaderNodeScalarSwitch::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}

VisualShaderNodeScalarSwitch::VisualShaderNodeScalarSwitch() {
    set_input_port_default_value(0, false);
    set_input_port_default_value(1, 1.0);
    set_input_port_default_value(2, 0.0);
}

////////////// Fresnel

se_string_view VisualShaderNodeFresnel::get_caption() const {
    return ("Fresnel");
}

int VisualShaderNodeFresnel::get_input_port_count() const {
    return 4;
}

VisualShaderNodeFresnel::PortType VisualShaderNodeFresnel::get_input_port_type(int p_port) const {
    switch (p_port) {
        case 0:
            return PORT_TYPE_VECTOR;
        case 1:
            return PORT_TYPE_VECTOR;
        case 2:
            return PORT_TYPE_BOOLEAN;
        case 3:
            return PORT_TYPE_SCALAR;
        default:
            return PORT_TYPE_VECTOR;
    }
}

StringName VisualShaderNodeFresnel::get_input_port_name(int p_port) const {
    switch (p_port) {
        case 0:
            return StringName("normal");
        case 1:
            return StringName("view");
        case 2:
            return StringName("invert");
        case 3:
            return StringName("power");
        default:
            return StringName();
    }
}

int VisualShaderNodeFresnel::get_output_port_count() const {
    return 1;
}

VisualShaderNodeFresnel::PortType VisualShaderNodeFresnel::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeFresnel::get_output_port_name(int p_port) const {
    return StringName("result");
}

String VisualShaderNodeFresnel::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    String normal;
    String view;
    if (p_input_vars[0].empty()) {
        normal = "NORMAL";
    } else {
        normal = p_input_vars[0];
    }
    if (p_input_vars[1].empty()) {
        view = "VIEW";
    } else {
        view = p_input_vars[1];
    }

    return "\t" + p_output_vars[0] + " = " + p_input_vars[2] + " ? (pow(clamp(dot(" + normal + ", " + view + "), 0.0, 1.0), " + p_input_vars[3] + ")) : (pow(1.0 - clamp(dot(" + normal + ", " + view + "), 0.0, 1.0), " + p_input_vars[3] + "));";
}

StringName VisualShaderNodeFresnel::get_input_port_default_hint(int p_port) const {
    if (p_port == 0) {
        return "default";
    } else if (p_port == 1) {
        return "default";
    }
    return StringName();
}

VisualShaderNodeFresnel::VisualShaderNodeFresnel() {
    set_input_port_default_value(2, false);
    set_input_port_default_value(3, 1.0);
}

////////////// Is

se_string_view VisualShaderNodeIs::get_caption() const {

    return ("Is");
}

int VisualShaderNodeIs::get_input_port_count() const {

    return 1;
}

VisualShaderNodeIs::PortType VisualShaderNodeIs::get_input_port_type(int p_port) const {

    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeIs::get_input_port_name(int p_port) const {

    return StringName();
}

int VisualShaderNodeIs::get_output_port_count() const {

    return 1;
}

VisualShaderNodeIs::PortType VisualShaderNodeIs::get_output_port_type(int p_port) const {

    return PORT_TYPE_BOOLEAN;
}

StringName VisualShaderNodeIs::get_output_port_name(int p_port) const {

    return StringName();
}

String VisualShaderNodeIs::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    static const char *funcs[FUNC_IS_NAN + 1] = {
        "isinf($)",
        "isnan($)"
    };

    String code;
    code += "\t" + p_output_vars[0] + "=" + StringUtils::replace((funcs[func]),("$"), p_input_vars[0]) + ";\n";
    return code;
}

void VisualShaderNodeIs::set_function(Function p_func) {

    func = p_func;
    emit_changed();
}

VisualShaderNodeIs::Function VisualShaderNodeIs::get_function() const {

    return func;
}

Vector<StringName> VisualShaderNodeIs::get_editable_properties() const {

    Vector<StringName> props;
    props.push_back("function");
    return props;
}

void VisualShaderNodeIs::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_function", {"func"}), &VisualShaderNodeIs::set_function);
    MethodBinder::bind_method(D_METHOD("get_function"), &VisualShaderNodeIs::get_function);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "function", PropertyHint::Enum, "Inf,NaN"), "set_function", "get_function");

    BIND_ENUM_CONSTANT(FUNC_IS_INF)
    BIND_ENUM_CONSTANT(FUNC_IS_NAN)
}

VisualShaderNodeIs::VisualShaderNodeIs() {

    func = FUNC_IS_INF;
    set_input_port_default_value(0, 0.0);
}

////////////// Compare

se_string_view VisualShaderNodeCompare::get_caption() const {

    return ("Compare");
}

int VisualShaderNodeCompare::get_input_port_count() const {

    if (ctype == CTYPE_SCALAR && (func == FUNC_EQUAL || func == FUNC_NOT_EQUAL)) {
        return 3;
    }
    return 2;
}

VisualShaderNodeCompare::PortType VisualShaderNodeCompare::get_input_port_type(int p_port) const {

    if (p_port == 2)
        return PORT_TYPE_SCALAR;
    switch (ctype) {
        case CTYPE_SCALAR:
            return PORT_TYPE_SCALAR;
        case CTYPE_VECTOR:
            return PORT_TYPE_VECTOR;
        case CTYPE_BOOLEAN:
            return PORT_TYPE_BOOLEAN;
        case CTYPE_TRANSFORM:
            return PORT_TYPE_TRANSFORM;
    }
    return PORT_TYPE_VECTOR;
}

StringName VisualShaderNodeCompare::get_input_port_name(int p_port) const {
    if (p_port == 0)
        return StringName("a");
    else if (p_port == 1)
        return StringName("b");
    else if (p_port == 2)
        return StringName("tolerance");
    return StringName();
}

int VisualShaderNodeCompare::get_output_port_count() const {
    return 1;
}

VisualShaderNodeCompare::PortType VisualShaderNodeCompare::get_output_port_type(int p_port) const {
    return PORT_TYPE_BOOLEAN;
}

StringName VisualShaderNodeCompare::get_output_port_name(int p_port) const {
    if (p_port == 0)
        return StringName("result");
    return StringName();
}

StringName VisualShaderNodeCompare::get_warning(ShaderMode p_mode, VisualShader::Type p_type) const {

    if (ctype == CTYPE_BOOLEAN || ctype == CTYPE_TRANSFORM) {
        if (func > FUNC_NOT_EQUAL) {
            return TTR("Invalid comparison function for that type.");
        }
    }

    return StringName();
}

String VisualShaderNodeCompare::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    static const char *ops[FUNC_LESS_THAN_EQUAL + 1] = {
        "==",
        "!=",
        ">",
        ">=",
        "<",
        "<=",
    };

    static const char *funcs[FUNC_LESS_THAN_EQUAL + 1] = {
        "equal($)",
        "notEqual($)",
        "greaterThan($)",
        "greaterThanEqual($)",
        "lessThan($)",
        "lessThanEqual($)",
    };

    static const char *conds[COND_ANY + 1] = {
        "all($)",
        "any($)",
    };

    String code;
    switch (ctype) {
        case CTYPE_SCALAR:
            if (func == FUNC_EQUAL) {
                code += "\t" + p_output_vars[0] + "=(abs(" + p_input_vars[0] + "-" + p_input_vars[1] + ")<" + p_input_vars[2] + ");";
            } else if (func == FUNC_NOT_EQUAL) {
                code += "\t" + p_output_vars[0] + "=!(abs(" + p_input_vars[0] + "-" + p_input_vars[1] + ")<" + p_input_vars[2] + ");";
            } else {
                code += "\t" + p_output_vars[0] + "=" + StringUtils::replace(p_input_vars[0] + "$" + p_input_vars[1],"$", ops[func]) + ";\n";
            }
            break;

        case CTYPE_VECTOR:
            code += ("\t{\n");
            code += "\t\tbvec3 _bv=" + StringUtils::replace((funcs[func]),("$"), p_input_vars[0] + ", " + p_input_vars[1]) + ";\n";
            code += "\t\t" + p_output_vars[0] + "=" + StringUtils::replace((conds[condition]),("$"), ("_bv")) + ";\n";
            code += ("\t}\n");
            break;

        case CTYPE_BOOLEAN:
            if (func > FUNC_NOT_EQUAL)
                return "\t" + p_output_vars[0] + "=false;\n";
            code += "\t" + p_output_vars[0] + "=" + StringUtils::replace(p_input_vars[0] + "$" + p_input_vars[1],"$", ops[func]) + ";\n";
            break;

        case CTYPE_TRANSFORM:
            if (func > FUNC_NOT_EQUAL)
                return "\t" + p_output_vars[0] + "=false;\n";
            code += "\t" + p_output_vars[0] + "=" + StringUtils::replace(p_input_vars[0] + "$" + p_input_vars[1],"$", ops[func]) + ";\n";
            break;

        default:
            break;
    }
    return code;
}

void VisualShaderNodeCompare::set_comparison_type(ComparisonType p_type) {

    ctype = p_type;

    switch (ctype) {
        case CTYPE_SCALAR:
            set_input_port_default_value(0, 0.0);
            set_input_port_default_value(1, 0.0);
            break;
        case CTYPE_VECTOR:
            set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0));
            set_input_port_default_value(1, Vector3(0.0, 0.0, 0.0));
            break;
        case CTYPE_BOOLEAN:
            set_input_port_default_value(0, false);
            set_input_port_default_value(1, false);
            break;
        case CTYPE_TRANSFORM:
            set_input_port_default_value(0, Transform());
            set_input_port_default_value(1, Transform());
            break;
    }
    emit_changed();
}

VisualShaderNodeCompare::ComparisonType VisualShaderNodeCompare::get_comparsion_type() const {

    return ctype;
}

void VisualShaderNodeCompare::set_function(Function p_func) {

    func = p_func;
    emit_changed();
}

VisualShaderNodeCompare::Function VisualShaderNodeCompare::get_function() const {

    return func;
}

void VisualShaderNodeCompare::set_condition(Condition p_cond) {

    condition = p_cond;
    emit_changed();
}

VisualShaderNodeCompare::Condition VisualShaderNodeCompare::get_condition() const {

    return condition;
}

Vector<StringName> VisualShaderNodeCompare::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("type");
    props.push_back("function");
    if (ctype == CTYPE_VECTOR)
        props.push_back("condition");
    return props;
}

void VisualShaderNodeCompare::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_comparison_type", {"type"}), &VisualShaderNodeCompare::set_comparison_type);
    MethodBinder::bind_method(D_METHOD("get_comparison_type"), &VisualShaderNodeCompare::get_comparsion_type);

    MethodBinder::bind_method(D_METHOD("set_function", {"func"}), &VisualShaderNodeCompare::set_function);
    MethodBinder::bind_method(D_METHOD("get_function"), &VisualShaderNodeCompare::get_function);

    MethodBinder::bind_method(D_METHOD("set_condition", {"condition"}), &VisualShaderNodeCompare::set_condition);
    MethodBinder::bind_method(D_METHOD("get_condition"), &VisualShaderNodeCompare::get_condition);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "type", PropertyHint::Enum, "Scalar,Vector,Boolean,Transform"), "set_comparison_type", "get_comparison_type");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "function", PropertyHint::Enum, "a == b,a != b,a > b,a >= b,a < b,a <= b"), "set_function", "get_function");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "condition", PropertyHint::Enum, "All,Any"), "set_condition", "get_condition");

    BIND_ENUM_CONSTANT(CTYPE_SCALAR)
    BIND_ENUM_CONSTANT(CTYPE_VECTOR)
    BIND_ENUM_CONSTANT(CTYPE_BOOLEAN)
    BIND_ENUM_CONSTANT(CTYPE_TRANSFORM)

    BIND_ENUM_CONSTANT(FUNC_EQUAL)
    BIND_ENUM_CONSTANT(FUNC_NOT_EQUAL)
    BIND_ENUM_CONSTANT(FUNC_GREATER_THAN)
    BIND_ENUM_CONSTANT(FUNC_GREATER_THAN_EQUAL)
    BIND_ENUM_CONSTANT(FUNC_LESS_THAN)
    BIND_ENUM_CONSTANT(FUNC_LESS_THAN_EQUAL)

    BIND_ENUM_CONSTANT(COND_ALL)
    BIND_ENUM_CONSTANT(COND_ANY)
}

VisualShaderNodeCompare::VisualShaderNodeCompare() {
    ctype = CTYPE_SCALAR;
    func = FUNC_EQUAL;
    condition = COND_ALL;
    set_input_port_default_value(0, 0.0);
    set_input_port_default_value(1, 0.0);
    set_input_port_default_value(2, CMP_EPSILON);
}
