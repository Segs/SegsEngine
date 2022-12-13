/*************************************************************************/
/*  shader_compiler_gles3.cpp                                            */
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

#include "shader_compiler_gles3.h"

#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/print_string.h"
#include "core/string_utils.h"
#include "core/string_formatter.h"

#define SL ShaderLanguage

using namespace eastl;

static String _mktab(int p_level) {

    String tb;
    for (int i = 0; i < p_level; i++) {
        tb += "\t";
    }

    return tb;
}

static String _constr(bool p_is_const) {
    if (p_is_const) {
        return "const ";
    }
    return "";
}
static String _typestr(SL::DataType p_type) {

    return ShaderLanguage::get_datatype_name(p_type);
}

static int _get_datatype_size(SL::DataType p_type) {

    switch (p_type) {

        case SL::TYPE_VOID:
            return 0;
        case SL::TYPE_BOOL:
            return 4;
        case SL::TYPE_BVEC2:
            return 8;
        case SL::TYPE_BVEC3:
            return 12;
        case SL::TYPE_BVEC4:
            return 16;
        case SL::TYPE_INT:
            return 4;
        case SL::TYPE_IVEC2:
            return 8;
        case SL::TYPE_IVEC3:
            return 12;
        case SL::TYPE_IVEC4:
            return 16;
        case SL::TYPE_UINT:
            return 4;
        case SL::TYPE_UVEC2:
            return 8;
        case SL::TYPE_UVEC3:
            return 12;
        case SL::TYPE_UVEC4:
            return 16;
        case SL::TYPE_FLOAT:
            return 4;
        case SL::TYPE_VEC2:
            return 8;
        case SL::TYPE_VEC3:
            return 12;
        case SL::TYPE_VEC4:
            return 16;
        case SL::TYPE_MAT2:
            return 32; //4 * 4 + 4 * 4
        case SL::TYPE_MAT3:
            return 48; // 4 * 4 + 4 * 4 + 4 * 4
        case SL::TYPE_MAT4:
            return 64;
        case SL::TYPE_SAMPLER2D:
            return 16;
        case SL::TYPE_ISAMPLER2D:
            return 16;
        case SL::TYPE_USAMPLER2D:
            return 16;
        case SL::TYPE_SAMPLER2DARRAY:
            return 16;
        case SL::TYPE_ISAMPLER2DARRAY:
            return 16;
        case SL::TYPE_USAMPLER2DARRAY:
            return 16;
        case SL::TYPE_SAMPLER3D:
            return 16;
        case SL::TYPE_ISAMPLER3D:
            return 16;
        case SL::TYPE_USAMPLER3D:
            return 16;
        case SL::TYPE_SAMPLERCUBE:
            return 16;
        case SL::TYPE_SAMPLEREXT:
            return 16;
        case SL::TYPE_STRUCT:
            return 0;
    }

    ERR_FAIL_V(0);
}

static int _get_datatype_alignment(SL::DataType p_type) {

    switch (p_type) {

        case SL::TYPE_VOID:
            return 0;
        case SL::TYPE_BOOL:
            return 4;
        case SL::TYPE_BVEC2:
            return 8;
        case SL::TYPE_BVEC3:
            return 16;
        case SL::TYPE_BVEC4:
            return 16;
        case SL::TYPE_INT:
            return 4;
        case SL::TYPE_IVEC2:
            return 8;
        case SL::TYPE_IVEC3:
            return 16;
        case SL::TYPE_IVEC4:
            return 16;
        case SL::TYPE_UINT:
            return 4;
        case SL::TYPE_UVEC2:
            return 8;
        case SL::TYPE_UVEC3:
            return 16;
        case SL::TYPE_UVEC4:
            return 16;
        case SL::TYPE_FLOAT:
            return 4;
        case SL::TYPE_VEC2:
            return 8;
        case SL::TYPE_VEC3:
            return 16;
        case SL::TYPE_VEC4:
            return 16;
        case SL::TYPE_MAT2:
            return 16;
        case SL::TYPE_MAT3:
            return 16;
        case SL::TYPE_MAT4:
            return 16;
        case SL::TYPE_SAMPLER2D:
            return 16;
        case SL::TYPE_ISAMPLER2D:
            return 16;
        case SL::TYPE_USAMPLER2D:
            return 16;
        case SL::TYPE_SAMPLER2DARRAY:
            return 16;
        case SL::TYPE_ISAMPLER2DARRAY:
            return 16;
        case SL::TYPE_USAMPLER2DARRAY:
            return 16;
        case SL::TYPE_SAMPLER3D:
            return 16;
        case SL::TYPE_ISAMPLER3D:
            return 16;
        case SL::TYPE_USAMPLER3D:
            return 16;
        case SL::TYPE_SAMPLERCUBE:
            return 16;
        case SL::TYPE_SAMPLEREXT:
            return 16;
        case SL::TYPE_STRUCT:
            return 0;
    }

    ERR_FAIL_V(0);
}
static StringView _interpstr(SL::DataInterpolation p_interp) {

    switch (p_interp) {
        case SL::INTERPOLATION_FLAT: return "flat "_sv;
        case SL::INTERPOLATION_SMOOTH:  return "";
    }
    return "";
}

static StringView _prestr(SL::DataPrecision p_pres) {

    switch (p_pres) {
        case SL::PRECISION_LOWP: return "lowp ";
        case SL::PRECISION_MEDIUMP: return "mediump ";
        case SL::PRECISION_HIGHP: return "highp ";
        case SL::PRECISION_DEFAULT: return "";
    }
    return "";
}

static StringView _qualstr(SL::ArgumentQualifier p_qual) {

    switch (p_qual) {
        case SL::ARGUMENT_QUALIFIER_IN: return "";
        case SL::ARGUMENT_QUALIFIER_OUT: return "out ";
        case SL::ARGUMENT_QUALIFIER_INOUT: return "inout ";
    }
    return "";
}

static StringView _opstr(SL::Operator p_op) {

    return SL::get_operator_text(p_op);
}

static String _mkid(const String &p_id) {

    String id = ("m_" + p_id).replaced("__", "_dus_");//doubleunderscore is reserved in glsl
    return id;
}

static String f2sp0(float p_float) {

    String num = StringUtils::num_scientific(p_float);
    if (not num.contains('.') && not num.contains('e')) {
        num += ".0";
    }
    return num;
}

static String get_constant_text(SL::DataType p_type, const Vector<SL::ConstantNode::Value> &p_values) {

    switch (p_type) {
        case SL::TYPE_BOOL: return p_values[0].boolean ? "true" : "false";
        case SL::TYPE_BVEC2:
        case SL::TYPE_BVEC3:
        case SL::TYPE_BVEC4: {

            String text = "bvec" + ::to_string(int(p_type - SL::TYPE_BOOL + 1)) + "(";
            for (size_t i = 0; i < p_values.size(); i++) {
                if (i > 0) {
                    text += ",";

                }
                text += p_values[i].boolean ? "true" : "false";
            }
            text += ")";
            return text;
        }

        case SL::TYPE_INT: return ::to_string(p_values[0].sint);
        case SL::TYPE_IVEC2:
        case SL::TYPE_IVEC3:
        case SL::TYPE_IVEC4: {

            String text = "ivec" + ::to_string(p_type - SL::TYPE_INT + 1) + "(";
            for (size_t i = 0; i < p_values.size(); i++) {
                if (i > 0) {
                    text += ",";
                }

                text += ::to_string(p_values[i].sint);
            }
            text += ")";
            return text;

        }
        case SL::TYPE_UINT: return ::to_string(p_values[0].uint) + "u";
        case SL::TYPE_UVEC2:
        case SL::TYPE_UVEC3:
        case SL::TYPE_UVEC4: {

            String text = "uvec" + ::to_string(p_type - SL::TYPE_UINT + 1) + "(";
            for (size_t i = 0; i < p_values.size(); i++) {
                if (i > 0) {
                    text += ",";
                }

                text += ::to_string(p_values[i].uint) + "u";
            }
            text += ")";
            return text;
        }
        case SL::TYPE_FLOAT:
            return f2sp0(p_values[0].real);
        case SL::TYPE_VEC2:
        case SL::TYPE_VEC3:
        case SL::TYPE_VEC4: {

            String text = "vec" + ::to_string(p_type - SL::TYPE_FLOAT + 1) + "(";
            for (size_t i = 0; i < p_values.size(); i++) {
                if (i > 0) {
                    text += ",";

                }
                text += f2sp0(p_values[i].real);
            }
            text += ")";
            return text;

        }
        case SL::TYPE_MAT2:
        case SL::TYPE_MAT3:
        case SL::TYPE_MAT4: {

            String text = "mat" + ::to_string(p_type - SL::TYPE_MAT2 + 2) + "(";
            for (size_t i = 0; i < p_values.size(); i++) {
                if (i > 0) {
                    text += ",";

                }
                text += f2sp0(p_values[i].real);
            }
            text += ")";
            return text;

        }
        default:
            ERR_FAIL_V(String());
    }
}

void ShaderCompilerGLES3::_dump_function_deps(const SL::ShaderNode *p_node, const StringName &p_for_func, const HashMap<StringName, String> &p_func_code, String &r_to_add, HashSet<StringName> &added) {

    int fidx = -1;

    for (size_t i = 0; i < p_node->functions.size(); i++) {
        if (p_node->functions[i].name == p_for_func) {
            fidx = i;
            break;
        }
    }

    ERR_FAIL_COND(fidx == -1);

    for (const StringName &E : p_node->functions[fidx].uses_function) {

        if (added.contains(E)) {
            continue; //was added already
        }

        _dump_function_deps(p_node, E, p_func_code, r_to_add, added);

        SL::FunctionNode *fnode = nullptr;

        for (auto & function : p_node->functions) {
            if (function.name == E) {
                fnode = function.function;
                break;
            }
        }

        ERR_FAIL_COND(!fnode);

        r_to_add += "\n";

        String header;
        if (fnode->return_type == SL::TYPE_STRUCT) {
            header = _mkid(fnode->return_struct_name.asCString()) + " " + _mkid(fnode->name.asCString()) + "(";
        } else {
        header = _typestr(fnode->return_type) + " " + _mkid(fnode->name.asCString()) + "(";
        }
        size_t arg_idx=0;
        for (const auto &arg : fnode->arguments) {

            if (arg_idx > 0) {
                header += ", ";
            }

            header += _constr(arg.is_const);

            if (arg.type == SL::TYPE_STRUCT) {
                header += String(_qualstr(arg.qualifier)) + _mkid(arg.type_str.asCString()) + " " + _mkid(arg.name.asCString());
            } else {
                header += String(_qualstr(arg.qualifier)) + _prestr(arg.precision) + _typestr(arg.type) + " " + _mkid(arg.name.asCString());
            }
            arg_idx++;
        }

        header += ")\n";
        r_to_add += header;
        r_to_add += p_func_code.at(E);

        added.insert(E);
    }
}

String ShaderCompilerGLES3::_dump_node_code(const SL::Node *p_node, int p_level, GeneratedCode &r_gen_code, IdentifierActions &p_actions, const DefaultIdentifierActions &p_default_actions, bool p_assigning, bool p_use_scope) {

    String code;

    switch (p_node->type) {

        case SL::Node::TYPE_SHADER: {

            SL::ShaderNode *pnode = (SL::ShaderNode *)p_node;

            for (auto & render_mode : pnode->render_modes) {

                if (p_default_actions.render_mode_defines.contains(render_mode) && !used_rmode_defines.contains(render_mode)) {

                    r_gen_code.defines.push_back(p_default_actions.render_mode_defines.at(render_mode).c_str());
                    used_rmode_defines.insert(render_mode);
                }

                if (p_actions.render_mode_flags.contains(render_mode)) {
                    *p_actions.render_mode_flags[render_mode] = true;
                }

                if (p_actions.render_mode_values.contains(render_mode)) {
                    Pair<int8_t *, int> &p = p_actions.render_mode_values[render_mode];
                    *p.first = p.second;
                }
            }
            // structs

            for (int i = 0; i < pnode->vstructs.size(); i++) {
                SL::StructNode *st = pnode->vstructs[i].shader_struct;
                String struct_code;

                struct_code += "struct ";
                struct_code += _mkid(pnode->vstructs[i].name.asCString());
                struct_code += " ";
                struct_code += "{\n";
                for (int j = 0; j < st->members.size(); j++) {
                    SL::MemberNode *m = st->members[j];
                    if (m->datatype == SL::TYPE_STRUCT) {
                        struct_code += _mkid(m->struct_name.asCString());
                    } else {
                        struct_code += _prestr(m->precision);
                        struct_code += _typestr(m->datatype);
                    }
                    struct_code += " ";
                    struct_code += m->name;
                    if (m->array_size > 0) {
                        struct_code += "[";
                        struct_code += itos(m->array_size);
                        struct_code += "]";
                    }
                    struct_code += ";\n";
                }
                struct_code += "}";
                struct_code += ";\n";

                r_gen_code.vertex_global += struct_code;
                r_gen_code.fragment_global += struct_code;
            }
            int max_texture_uniforms = 0;
            int max_uniforms = 0;

            for (eastl::pair<const StringName,SL::ShaderNode::Uniform> &E : pnode->uniforms) {
                if (SL::is_sampler_type(E.second.type)) {
                    max_texture_uniforms++;
                } else {
                    max_uniforms++;
                }
            }

            r_gen_code.texture_uniforms.resize(max_texture_uniforms);
            r_gen_code.texture_hints.resize(max_texture_uniforms);
            r_gen_code.texture_types.resize(max_texture_uniforms);

            FixedVector<int,256> uniform_sizes(max_uniforms);
            FixedVector<int, 256> uniform_alignments(max_uniforms);
            FixedVector<String, 256> uniform_defines(max_uniforms);

            bool uses_uniforms = false;

            for (eastl::pair<const StringName,SL::ShaderNode::Uniform> &E : pnode->uniforms) {

                String ucode;

                if (SL::is_sampler_type(E.second.type)) {
                    ucode = "uniform ";
                }

                ucode.append(_prestr(E.second.precision));
                ucode += _typestr(E.second.type) + " "+_mkid(E.first.asCString())+";\n";
                if (SL::is_sampler_type(E.second.type)) {
                    r_gen_code.vertex_global += ucode;
                    r_gen_code.fragment_global += ucode;
                    r_gen_code.texture_uniforms[E.second.texture_order] = StringName(_mkid(E.first.asCString()).c_str());
                    r_gen_code.texture_hints[E.second.texture_order] = E.second.hint;
                    r_gen_code.texture_types[E.second.texture_order] = E.second.type;
                } else {
                    if (!uses_uniforms) {

                        r_gen_code.defines.push_back("#define USE_MATERIAL\n");
                        uses_uniforms = true;
                    }
                    uniform_defines[E.second.order] = eastl::move(ucode);
                    uniform_sizes[E.second.order] = _get_datatype_size(E.second.type);
                    uniform_alignments[E.second.order] = _get_datatype_alignment(E.second.type);
                }

                p_actions.uniforms->emplace(E.first, E.second);
            }

            for (int i = 0; i < max_uniforms; i++) {
                r_gen_code.uniforms += uniform_defines[i];
            }

            // add up
            int offset = 0;
            for (int i = 0; i < max_uniforms; i++) {

                int align = offset % uniform_alignments[i];

                if (align != 0) {
                    offset += uniform_alignments[i] - align;
                }

                r_gen_code.uniform_offsets.push_back(offset);

                offset += uniform_sizes[i];
            }

            r_gen_code.uniform_total_size = offset;
            if (r_gen_code.uniform_total_size % 16 != 0) { //UBO sizes must be multiples of 16
                r_gen_code.uniform_total_size += r_gen_code.uniform_total_size % 16;
            }

            Vector<Pair<StringName, SL::ShaderNode::Varying>> var_frag_to_light;
            for (eastl::pair<const StringName,SL::ShaderNode::Varying> &E : pnode->varyings) {

                if (E.second.stage == SL::ShaderNode::Varying::STAGE_FRAGMENT_TO_LIGHT || E.second.stage == SL::ShaderNode::Varying::STAGE_FRAGMENT) {
                    var_frag_to_light.emplace_back(E);
                    fragment_varyings.insert(E.first);
                    continue;
                }
                String vcode;
                String interp_mode(_interpstr(E.second.interpolation));
                vcode.append(_prestr(E.second.precision));
                vcode += _typestr(E.second.type) + " " + _mkid(E.first.asCString());
                if (E.second.array_size > 0) {
                    vcode += "[" + ::to_string(E.second.array_size) + "]";
                }
                vcode += ";\n";
                r_gen_code.vertex_global += interp_mode + "out " + vcode;
                r_gen_code.fragment_global += interp_mode + "in " + vcode;
            }

            if (!var_frag_to_light.empty()) {
                String gcode = "\n\nstruct {\n";
                for (const Pair<StringName, SL::ShaderNode::Varying> &E : var_frag_to_light) {
                    gcode += "\t" + _prestr(E.second.precision) + _typestr(E.second.type) + " " + _mkid(E.first.asCString());
                    if (E.second.array_size > 0) {
                        gcode += "[";
                        gcode += itos(E.second.array_size);
                        gcode += "]";
                    }
                    gcode += ";\n";
                }
                gcode += "} frag_to_light;\n";
                r_gen_code.fragment_global += gcode;
            }
            for (const auto &vconstant : pnode->vconstants) {
                String gcode;
                gcode += _constr(true);
                if (vconstant.type == SL::TYPE_STRUCT) {
                    gcode += _mkid(vconstant.type_str.asCString());
                } else {
                    gcode += _prestr(vconstant.precision);
                    gcode += _typestr(vconstant.type);
                }
                gcode += " " + _mkid(String(vconstant.name));
                if (vconstant.array_size > 0) {
                    gcode += "[";
                    gcode += itos(vconstant.array_size);
                    gcode += "]";
                }
                gcode += "=";
                gcode += _dump_node_code(vconstant.initializer, p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                gcode += ";\n";
                r_gen_code.vertex_global += gcode;
                r_gen_code.fragment_global += gcode;
            }

            HashMap<StringName, String> function_code;

            //code for functions
            for (auto & f : pnode->functions) {
                SL::FunctionNode *fnode = f.function;
                function = fnode;
                current_func_name = fnode->name;
                function_code[fnode->name] = _dump_node_code(fnode->body, p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
                function = nullptr;
            }

            //place functions in actual code

            HashSet<StringName> added_vtx;
            HashSet<StringName> added_fragment; //share for light

            for (size_t i = 0; i < pnode->functions.size(); i++) {

                SL::FunctionNode *fnode = pnode->functions[i].function;

                function = fnode;
                current_func_name = fnode->name;

                if (fnode->name == vertex_name) {

                    _dump_function_deps(pnode, fnode->name, function_code, r_gen_code.vertex_global, added_vtx);
                    r_gen_code.vertex = function_code[vertex_name];
                }

                if (fnode->name == fragment_name) {

                    _dump_function_deps(pnode, fnode->name, function_code, r_gen_code.fragment_global, added_fragment);
                    r_gen_code.fragment = function_code[fragment_name];
                }

                if (fnode->name == light_name) {

                    _dump_function_deps(pnode, fnode->name, function_code, r_gen_code.fragment_global, added_fragment);
                    r_gen_code.light = function_code[light_name];
                }
                function = nullptr;
            }

            //code+=dump_node_code(pnode->body,p_level);
        } break;
        case SL::Node::TYPE_STRUCT: {
        } break;
        case SL::Node::TYPE_FUNCTION: {

        } break;
        case SL::Node::TYPE_BLOCK: {
            SL::BlockNode *bnode = (SL::BlockNode *)p_node;

            //variables
            if (!bnode->single_statement) {
                code += _mktab(p_level - 1) + "{\n";
            }

            for (int i = 0; i < bnode->statements.size(); i++) {

                String scode = _dump_node_code(bnode->statements[i], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);

                if (bnode->statements[i]->type == SL::Node::TYPE_CONTROL_FLOW || bnode->single_statement) {
                    code += scode; //use directly
                } else {
                    code += _mktab(p_level) + scode + ";\n";
                }
            }
            if (!bnode->single_statement) {
                code += _mktab(p_level - 1) + "}\n";
            }

        } break;
        case SL::Node::TYPE_VARIABLE_DECLARATION: {
            SL::VariableDeclarationNode *vdnode = (SL::VariableDeclarationNode *)p_node;

            String declaration = _constr(vdnode->is_const);
            if (vdnode->datatype == SL::TYPE_STRUCT) {
                declaration += _mkid(vdnode->struct_name.asCString());
            } else {
                declaration += _prestr(vdnode->precision);
                declaration += _typestr(vdnode->datatype);
            }
            for (size_t i = 0; i < vdnode->declarations.size(); i++) {
                if (i > 0) {
                    declaration += ",";
                } else {
                    declaration += " ";
                }
                declaration += _mkid(vdnode->declarations[i].name.asCString());
                if (vdnode->declarations[i].initializer) {
                    declaration += "=";
                    declaration += _dump_node_code(vdnode->declarations[i].initializer, p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                }
            }

            code += declaration;
        } break;
        case SL::Node::TYPE_VARIABLE: {
            SL::VariableNode *vnode = (SL::VariableNode *)p_node;
            bool use_fragment_varying = false;

            if (!vnode->is_local && current_func_name != vertex_name) {
                if (p_assigning) {
                    if (shader->varyings.contains(vnode->name)) {
                        use_fragment_varying = true;
                    }
                } else {
                    if (fragment_varyings.contains(vnode->name)) {
                        use_fragment_varying = true;
                    }
                }
            }

            if (p_assigning && p_actions.write_flag_pointers.contains(vnode->name)) {
                *p_actions.write_flag_pointers[vnode->name] = true;
            }

            if (p_default_actions.usage_defines.contains(vnode->name) && !used_name_defines.contains(vnode->name)) {
                String define = p_default_actions.usage_defines.at(vnode->name);
                if (StringUtils::begins_with(define,"@")) {
                    define = p_default_actions.usage_defines.at(StringName(StringUtils::substr(define,1, define.length()).data()));
                }
                r_gen_code.defines.push_back(define.c_str());
                used_name_defines.insert(vnode->name);
            }

            if (p_actions.usage_flag_pointers.contains(vnode->name) && !used_flag_pointers.contains(vnode->name)) {
                *p_actions.usage_flag_pointers[vnode->name] = true;
                used_flag_pointers.insert(vnode->name);
            }

            if (p_default_actions.renames.contains(vnode->name)) {
                code = p_default_actions.renames.at(vnode->name);
            } else if (use_fragment_varying) {
                code = "frag_to_light." + _mkid(vnode->name.asCString());
            } else {
                code = _mkid(vnode->name.asCString());
            }

            if (vnode->name == time_name) {
                if (current_func_name == vertex_name) {
                    r_gen_code.uses_vertex_time = true;
                }
                if (current_func_name == fragment_name || current_func_name == light_name) {
                    r_gen_code.uses_fragment_time = true;
                }
            }

        } break;
        case SL::Node::TYPE_ARRAY_CONSTRUCT: {
            SL::ArrayConstructNode *acnode = (SL::ArrayConstructNode *)p_node;
            int sz = acnode->initializer.size();
            if (acnode->datatype == SL::TYPE_STRUCT) {
                code += _mkid(acnode->struct_name.asCString());
            } else {
                code += _typestr(acnode->datatype);
            }
            code += "[";
            code += itos(acnode->initializer.size());
            code += "]";
            code += "(";
            for (int i = 0; i < sz; i++) {
                code += _dump_node_code(acnode->initializer[i], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                if (i != sz - 1) {
                    code += ", ";
                }
            }
            code += ")";
        } break;
        case SL::Node::TYPE_ARRAY_DECLARATION: {

            SL::ArrayDeclarationNode *adnode = (SL::ArrayDeclarationNode *)p_node;

            String declaration;
            if (adnode->is_const) {
                declaration += "const ";
            }
            if (adnode->datatype == SL::TYPE_STRUCT) {
                declaration += _mkid(adnode->struct_name.asCString());
            } else {
            declaration += _prestr(adnode->precision);
            declaration += _typestr(adnode->datatype);
            }
            bool decl_started=false;
            for (auto &decl : adnode->declarations) {
                if (decl_started) {
                    declaration += ",";
                } else {
                    declaration += " ";
                }
                decl_started = true;
                declaration += _mkid(decl.name.asCString());
                declaration += "["+::to_string(decl.size)+"]";
                int sz = decl.initializer.size();
                if (sz > 0) {
                    declaration += "=";
                    if (adnode->datatype == SL::TYPE_STRUCT) {
                        declaration += _mkid(adnode->struct_name.asCString());
                    } else {
                    declaration += _typestr(adnode->datatype);
                    }
                    declaration += "["+::to_string(sz)+"](";
                    for (int j = 0; j < sz; j++) {
                        declaration += _dump_node_code(decl.initializer[j], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                        if (j != sz - 1) {
                            declaration += ", ";
                        }
                    }
                    declaration += ")";
                }
            }

            code += declaration;
        } break;
        case SL::Node::TYPE_ARRAY: {
            SL::ArrayNode *anode = (SL::ArrayNode *)p_node;
            bool use_fragment_varying = false;

            if (!anode->is_local && current_func_name != vertex_name) {
                if (anode->assign_expression != nullptr) {
                    use_fragment_varying = true;
                } else {
                    if (p_assigning) {
                        if (shader->varyings.contains(anode->name)) {
                            use_fragment_varying = true;
                        }
                    } else {
                        if (fragment_varyings.contains(anode->name)) {
                            use_fragment_varying = true;
                        }
                    }
                }
            }
            if (p_assigning && p_actions.write_flag_pointers.contains(anode->name)) {
                *p_actions.write_flag_pointers[anode->name] = true;
            }

            if (p_default_actions.usage_defines.contains(anode->name) && !used_name_defines.contains(anode->name)) {
                String define = p_default_actions.usage_defines.at(anode->name);
                if (StringUtils::begins_with(define,"@")) {
                    define = p_default_actions.usage_defines.at(StringName(StringUtils::substr(define, 1, define.length()).data()));
                }
                r_gen_code.defines.push_back(define.c_str());
                used_name_defines.insert(anode->name);
            }

            if (p_actions.usage_flag_pointers.contains(anode->name) && !used_flag_pointers.contains(anode->name)) {
                *p_actions.usage_flag_pointers[anode->name] = true;
                used_flag_pointers.insert(anode->name);
            }

            if (p_default_actions.renames.contains(anode->name)) {
                code = p_default_actions.renames.at(anode->name);
            } else if (use_fragment_varying) {
                code = "frag_to_light." + _mkid(anode->name.asCString());
            } else {
                code = _mkid(anode->name.asCString());
            }

            if (anode->call_expression != nullptr) {
                code += ".";
                code += _dump_node_code(anode->call_expression, p_level, r_gen_code, p_actions, p_default_actions, p_assigning,false);
            } else if (anode->index_expression != nullptr) {
                code += "[";
                code += _dump_node_code(anode->index_expression, p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                code += "]";
            } else if (anode->assign_expression != nullptr) {
                code += "=";
                code += _dump_node_code(anode->assign_expression, p_level, r_gen_code, p_actions, p_default_actions, true, false);
            }

            if (anode->name == time_name) {
                if (current_func_name == vertex_name) {
                    r_gen_code.uses_vertex_time = true;
                }
                if (current_func_name == fragment_name || current_func_name == light_name) {
                    r_gen_code.uses_fragment_time = true;
                }
            }

        } break;
        case SL::Node::TYPE_CONSTANT: {
            SL::ConstantNode *cnode = (SL::ConstantNode *)p_node;
            if (cnode->array_size == 0) {
            return get_constant_text(cnode->datatype, cnode->values);
            } else {
                if (cnode->get_datatype() == SL::TYPE_STRUCT) {
                    code += _mkid(cnode->struct_name.asCString());
                } else {
                    code += _typestr(cnode->datatype);
                }
                code += "[" + itos(cnode->array_size) + "](";
                for (int i = 0; i < cnode->array_size; i++) {
                    if (i > 0) {
                        code += ",";
                    } else {
                        code += "";
                    }
                    code += _dump_node_code(cnode->array_declarations[0].initializer[i], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                }
                code += ")";
            }

        } break;
        case SL::Node::TYPE_OPERATOR: {
            SL::OperatorNode *onode = (SL::OperatorNode *)p_node;

            switch (onode->op) {

                case SL::OP_ASSIGN:
                case SL::OP_ASSIGN_ADD:
                case SL::OP_ASSIGN_SUB:
                case SL::OP_ASSIGN_MUL:
                case SL::OP_ASSIGN_DIV:
                case SL::OP_ASSIGN_SHIFT_LEFT:
                case SL::OP_ASSIGN_SHIFT_RIGHT:
                case SL::OP_ASSIGN_MOD:
                case SL::OP_ASSIGN_BIT_AND:
                case SL::OP_ASSIGN_BIT_OR:
                case SL::OP_ASSIGN_BIT_XOR:
                    code = _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, true) + _opstr(onode->op) + _dump_node_code(onode->arguments[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                    break;
                case SL::OP_BIT_INVERT:
                case SL::OP_NEGATE:
                case SL::OP_NOT:
                case SL::OP_DECREMENT:
                case SL::OP_INCREMENT:
                    code = String(_opstr(onode->op)) + _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                    break;
                case SL::OP_POST_DECREMENT:
                case SL::OP_POST_INCREMENT:
                    code = _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + _opstr(onode->op);
                    break;
                case SL::OP_CALL:
                case SL::OP_STRUCT:
                case SL::OP_CONSTRUCT: {

                    ERR_FAIL_COND_V(onode->arguments[0]->type != SL::Node::TYPE_VARIABLE, String());

                    SL::VariableNode *vnode = (SL::VariableNode *)onode->arguments[0];

                    if (onode->op == SL::OP_STRUCT) {
                        code += _mkid(vnode->name.asCString());
                    } else if (onode->op == SL::OP_CONSTRUCT) {
                        code += vnode->name.asCString();
                    } else {

                        if (internal_functions.contains(vnode->name)) {
                            code += vnode->name.asCString();
                        } else if (p_default_actions.renames.contains(vnode->name)) {
                            code += p_default_actions.renames.at(vnode->name);
                        } else {
                            code += _mkid(vnode->name.asCString());
                        }
                    }

                    code += "(";

                    for (size_t i = 1; i < onode->arguments.size(); i++) {
                        if (i > 1) {
                            code += ", ";
                        }
                        code += _dump_node_code(onode->arguments[i], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                    }
                    code += ")";
                } break;
                case SL::OP_INDEX: {

                    code += _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                    code += "[";
                    code += _dump_node_code(onode->arguments[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                    code += "]";

                } break;
                case SL::OP_SELECT_IF: {

                    code += "(";
                    code += _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                    code += "?";
                    code += _dump_node_code(onode->arguments[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                    code += ":";
                    code += _dump_node_code(onode->arguments[2], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                    code += ")";

                } break;

                default: {

                if (p_use_scope) {
                    code += "(";
                }
                code += _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + _opstr(onode->op) + _dump_node_code(onode->arguments[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                    if (p_use_scope) {
                        code += ")";
                    }
                    break;
                }
            }

        } break;
        case SL::Node::TYPE_CONTROL_FLOW: {
            SL::ControlFlowNode *cfnode = (SL::ControlFlowNode *)p_node;
            if (cfnode->flow_op == SL::FLOW_OP_IF) {

                code += _mktab(p_level) + "if (" + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ")\n";
                code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
                if (cfnode->blocks.size() == 2) {

                    code += _mktab(p_level) + "else\n";
                    code += _dump_node_code(cfnode->blocks[1], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
                }
            } else if (cfnode->flow_op == SL::FLOW_OP_SWITCH) {
                code += _mktab(p_level) + "switch (" + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ")\n";
                code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
            } else if (cfnode->flow_op == SL::FLOW_OP_CASE) {
                code += _mktab(p_level) + "case " + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ":\n";
                code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
            } else if (cfnode->flow_op == SL::FLOW_OP_DEFAULT) {
                code += _mktab(p_level) + "default:\n";
                code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
            } else if (cfnode->flow_op == SL::FLOW_OP_DO) {
                code += _mktab(p_level) + "do";
                code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
                code += _mktab(p_level) + "while (" + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ");";

            } else if (cfnode->flow_op == SL::FLOW_OP_WHILE) {

                code += _mktab(p_level) + "while (" + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ")\n";
                code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
            } else if (cfnode->flow_op == SL::FLOW_OP_FOR) {

                String left = _dump_node_code(cfnode->blocks[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                String middle = _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                String right = _dump_node_code(cfnode->expressions[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                code += _mktab(p_level) + "for (" + left + ";" + middle + ";" + right + ")\n";
                code += _dump_node_code(cfnode->blocks[1], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);

            } else if (cfnode->flow_op == SL::FLOW_OP_RETURN) {

                if (!cfnode->expressions.empty()) {
                    code = "return " + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ";";
                } else {
                    code = "return;";
                }
            } else if (cfnode->flow_op == SL::FLOW_OP_DISCARD) {

                if (p_actions.usage_flag_pointers.contains("DISCARD") && !used_flag_pointers.contains("DISCARD")) {
                    *p_actions.usage_flag_pointers["DISCARD"] = true;
                    used_flag_pointers.insert("DISCARD");
                }

                code = "discard;";
            } else if (cfnode->flow_op == SL::FLOW_OP_CONTINUE) {

                code = "continue;";
            } else if (cfnode->flow_op == SL::FLOW_OP_BREAK) {

                code = "break;";
            }

        } break;
        case SL::Node::TYPE_MEMBER: {
            SL::MemberNode *mnode = (SL::MemberNode *)p_node;
            code = _dump_node_code(mnode->owner, p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + "." + mnode->name.asCString();
            if (mnode->index_expression != nullptr) {
                code += "[";
                code += _dump_node_code(mnode->index_expression, p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
                code += "]";
            } else if (mnode->assign_expression != nullptr) {
                code += "=";
                code += _dump_node_code(mnode->assign_expression, p_level, r_gen_code, p_actions, p_default_actions, true, false);
            }

        } break;
    }

    return code;
}

Error ShaderCompilerGLES3::compile(RS::ShaderMode p_mode, const String &p_code, IdentifierActions *p_actions, const String &p_path, GeneratedCode &r_gen_code) {

    Error err = parser.compile(p_code, ShaderTypes::get_singleton()->get_functions(p_mode),
            ShaderTypes::get_singleton()->get_modes(p_mode), ShaderTypes::get_singleton()->get_types());

    if (err != OK) {
        Vector<StringView> shader;
        String::split_ref(shader,p_code,'\n');

        for (size_t i = 0; i < shader.size(); i++) {
            if (i + 1 == parser.get_error_line()) {
                // Mark the error line to be visible without having to look at
                // the trace at the end.
                print_line(FormatVE("E%4lu-> %.*s", i + 1, (int)shader[i].size(),shader[i].data()));
            } else {
                print_line(FormatVE("%5lu | %.*s", i + 1, (int)shader[i].size(),shader[i].data()));
            }
        }

        _err_print_error({}, p_path.data(), parser.get_error_line(), parser.get_error_text(), {},ERR_HANDLER_SHADER);
        return err;
    }

    r_gen_code.defines.clear();
    r_gen_code.vertex = String();
    r_gen_code.vertex_global = String();
    r_gen_code.fragment = String();
    r_gen_code.fragment_global = String();
    r_gen_code.light = String();
    r_gen_code.uses_fragment_time = false;
    r_gen_code.uses_vertex_time = false;

    used_name_defines.clear();
    used_rmode_defines.clear();
    used_flag_pointers.clear();
    fragment_varyings.clear();

    shader = parser.get_shader();
    function = nullptr;
    _dump_node_code(shader, 1, r_gen_code, *p_actions, actions[(int)p_mode], false);

    if (r_gen_code.uniform_total_size) { //uniforms used?
        unsigned int md = sizeof(float) * 4;
        if (r_gen_code.uniform_total_size % md) {
            r_gen_code.uniform_total_size += md - (r_gen_code.uniform_total_size % md);
        }
        r_gen_code.uniform_total_size += md; //pad just in case
    }

    return OK;
}

ShaderCompilerGLES3::ShaderCompilerGLES3() {

    /** CANVAS ITEM SHADER **/
    auto &canvas_renames(actions[(int)RS::ShaderMode::CANVAS_ITEM].renames);
    canvas_renames["VERTEX"] = "outvec.xy";
    canvas_renames["UV"] = "uv";
    canvas_renames["POINT_SIZE"] = "point_size";

    canvas_renames["WORLD_MATRIX"] = "modelview_matrix";
    canvas_renames["PROJECTION_MATRIX"] = "projection_matrix";
    canvas_renames["EXTRA_MATRIX"] = "extra_matrix";
    canvas_renames["TIME"] = "time";
    canvas_renames["AT_LIGHT_PASS"] = "at_light_pass";
    canvas_renames["INSTANCE_CUSTOM"] = "instance_custom";

    canvas_renames["COLOR"] = "color";
    canvas_renames["MODULATE"] = "final_modulate_alias";
    canvas_renames["NORMAL"] = "normal";
    canvas_renames["NORMALMAP"] = "normal_map";
    canvas_renames["NORMALMAP_DEPTH"] = "normal_depth";
    canvas_renames["TEXTURE"] = "color_texture";
    canvas_renames["TEXTURE_PIXEL_SIZE"] = "color_texpixel_size";
    canvas_renames["NORMAL_TEXTURE"] = "normal_texture";
    canvas_renames["SCREEN_UV"] = "screen_uv";
    canvas_renames["SCREEN_TEXTURE"] = "screen_texture";
    canvas_renames["SCREEN_PIXEL_SIZE"] = "screen_pixel_size";
    canvas_renames["FRAGCOORD"] = "gl_FragCoord";
    canvas_renames["POINT_COORD"] = "gl_PointCoord";
    canvas_renames["INSTANCE_ID"] = "gl_InstanceID";
    canvas_renames["VERTEX_ID"] = "gl_VertexID";

    canvas_renames["LIGHT_VEC"] = "light_vec";
    canvas_renames["LIGHT_HEIGHT"] = "light_height";
    canvas_renames["LIGHT_COLOR"] = "light_color";
    canvas_renames["LIGHT_UV"] = "light_uv";
    canvas_renames["LIGHT"] = "light";
    canvas_renames["SHADOW_COLOR"] = "shadow_color";
    canvas_renames["SHADOW_VEC"] = "shadow_vec";

    auto &canvas_usages(actions[(int)RS::ShaderMode::CANVAS_ITEM].usage_defines);
    canvas_usages["COLOR"] = "#define COLOR_USED\n";
    canvas_usages["MODULATE"] = "#define MODULATE_USED\n";
    canvas_usages["SCREEN_TEXTURE"] = "#define SCREEN_TEXTURE_USED\n";
    canvas_usages["SCREEN_UV"] = "#define SCREEN_UV_USED\n";
    canvas_usages["SCREEN_PIXEL_SIZE"] = "@SCREEN_UV";
    canvas_usages["NORMAL"] = "#define NORMAL_USED\n";
    canvas_usages["NORMALMAP"] = "#define NORMALMAP_USED\n";
    canvas_usages["LIGHT"] = "#define USE_LIGHT_SHADER_CODE\n";
    canvas_usages["SHADOW_VEC"] = "#define SHADOW_VEC_USED\n";
    actions[(int)RS::ShaderMode::CANVAS_ITEM].render_mode_defines["skip_vertex_transform"] = "#define SKIP_TRANSFORM_USED\n";

    /** SPATIAL SHADER **/
    auto &spatial_renames(actions[(int)RS::ShaderMode::SPATIAL].renames);

    spatial_renames["WORLD_MATRIX"] = "world_transform";
    spatial_renames["INV_CAMERA_MATRIX"] = "camera_inverse_matrix";
    spatial_renames["CAMERA_MATRIX"] = "camera_matrix";
    spatial_renames["PROJECTION_MATRIX"] = "projection_matrix";
    spatial_renames["INV_PROJECTION_MATRIX"] = "inv_projection_matrix";
    spatial_renames["MODELVIEW_MATRIX"] = "modelview";

    spatial_renames["VIEW_INDEX"] = "view_index";
    spatial_renames["VIEW_MONO_LEFT"] = "0";
    spatial_renames["VIEW_RIGHT"] = "1";
    spatial_renames["VERTEX"] = "vertex.xyz";
    spatial_renames["NORMAL"] = "normal";
    spatial_renames["TANGENT"] = "tangent";
    spatial_renames["BINORMAL"] = "binormal";
    spatial_renames["POSITION"] = "position";
    spatial_renames["UV"] = "uv_interp";
    spatial_renames["UV2"] = "uv2_interp";
    spatial_renames["COLOR"] = "color_interp";
    spatial_renames["POINT_SIZE"] = "point_size";
    spatial_renames["INSTANCE_ID"] = "gl_InstanceID";
    spatial_renames["VERTEX_ID"] = "gl_VertexID";

    //builtins

    spatial_renames["TIME"] = "time";
    spatial_renames["VIEWPORT_SIZE"] = "viewport_size";

    spatial_renames["FRAGCOORD"] = "gl_FragCoord";
    spatial_renames["FRONT_FACING"] = "gl_FrontFacing";
    spatial_renames["NORMALMAP"] = "normalmap";
    spatial_renames["NORMALMAP_DEPTH"] = "normaldepth";
    spatial_renames["ALBEDO"] = "albedo";
    spatial_renames["ALPHA"] = "alpha";
    spatial_renames["METALLIC"] = "metallic";
    spatial_renames["SPECULAR"] = "specular";
    spatial_renames["ROUGHNESS"] = "roughness";
    spatial_renames["RIM"] = "rim";
    spatial_renames["RIM_TINT"] = "rim_tint";
    spatial_renames["CLEARCOAT"] = "clearcoat";
    spatial_renames["CLEARCOAT_GLOSS"] = "clearcoat_gloss";
    spatial_renames["ANISOTROPY"] = "anisotropy";
    spatial_renames["ANISOTROPY_FLOW"] = "anisotropy_flow";
    spatial_renames["SSS_STRENGTH"] = "sss_strength";
    spatial_renames["TRANSMISSION"] = "transmission";
    spatial_renames["AO"] = "ao";
    spatial_renames["AO_LIGHT_AFFECT"] = "ao_light_affect";
    spatial_renames["EMISSION"] = "emission";
    spatial_renames["POINT_COORD"] = "gl_PointCoord";
    spatial_renames["INSTANCE_CUSTOM"] = "instance_custom";
    spatial_renames["SCREEN_UV"] = "screen_uv";
    spatial_renames["SCREEN_TEXTURE"] = "screen_texture";
    spatial_renames["DEPTH_TEXTURE"] = "depth_buffer";
    spatial_renames["DEPTH"] = "gl_FragDepth";
    spatial_renames["ALPHA_SCISSOR"] = "alpha_scissor";
    spatial_renames["OUTPUT_IS_SRGB"] = "SHADER_IS_SRGB";
    spatial_renames["NODE_POSITION_WORLD"] = "world_transform[3].xyz";
    spatial_renames["CAMERA_POSITION_WORLD"] = "camera_matrix[3].xyz";
    spatial_renames["CAMERA_DIRECTION_WORLD"] = "camera_inverse_matrix[3].xyz";
    spatial_renames["NODE_POSITION_VIEW"] = "(world_transform * camera_inverse_matrix)[3].xyz";

    //for light
    spatial_renames["VIEW"] = "view";
    spatial_renames["LIGHT_COLOR"] = "light_color";
    spatial_renames["LIGHT"] = "light";
    spatial_renames["ATTENUATION"] = "attenuation";
    spatial_renames["DIFFUSE_LIGHT"] = "diffuse_light";
    spatial_renames["SPECULAR_LIGHT"] = "specular_light";

    auto &spatial_usages(actions[(int)RS::ShaderMode::SPATIAL].usage_defines);
    auto &spatial_rendermode_defs(actions[(int)RS::ShaderMode::SPATIAL].render_mode_defines);

    spatial_usages["TANGENT"] = "#define ENABLE_TANGENT_INTERP\n";
    spatial_usages["BINORMAL"] = "@TANGENT";
    spatial_usages["RIM"] = "#define LIGHT_USE_RIM\n";
    spatial_usages["RIM_TINT"] = "@RIM";
    spatial_usages["CLEARCOAT"] = "#define LIGHT_USE_CLEARCOAT\n";
    spatial_usages["CLEARCOAT_GLOSS"] = "@CLEARCOAT";
    spatial_usages["ANISOTROPY"] = "#define LIGHT_USE_ANISOTROPY\n";
    spatial_usages["ANISOTROPY_FLOW"] = "@ANISOTROPY";
    spatial_usages["AO"] = "#define ENABLE_AO\n";
    spatial_usages["AO_LIGHT_AFFECT"] = "#define ENABLE_AO\n";
    spatial_usages["UV"] = "#define ENABLE_UV_INTERP\n";
    spatial_usages["UV2"] = "#define ENABLE_UV2_INTERP\n";
    spatial_usages["NORMALMAP"] = "#define ENABLE_NORMALMAP\n";
    spatial_usages["NORMALMAP_DEPTH"] = "@NORMALMAP";
    spatial_usages["COLOR"] = "#define ENABLE_COLOR_INTERP\n";
    spatial_usages["INSTANCE_CUSTOM"] = "#define ENABLE_INSTANCE_CUSTOM\n";
    spatial_usages["ALPHA_SCISSOR"] = "#define ALPHA_SCISSOR_USED\n";
    spatial_usages["POSITION"] = "#define OVERRIDE_POSITION\n";

    spatial_usages["SSS_STRENGTH"] = "#define ENABLE_SSS\n";
    spatial_usages["TRANSMISSION"] = "#define TRANSMISSION_USED\n";
    spatial_usages["SCREEN_TEXTURE"] = "#define SCREEN_TEXTURE_USED\n";
    spatial_usages["SCREEN_UV"] = "#define SCREEN_UV_USED\n";

    spatial_usages["DIFFUSE_LIGHT"] = "#define USE_LIGHT_SHADER_CODE\n";
    spatial_usages["SPECULAR_LIGHT"] = "#define USE_LIGHT_SHADER_CODE\n";

    spatial_rendermode_defs["skip_vertex_transform"] = "#define SKIP_TRANSFORM_USED\n";
    spatial_rendermode_defs["world_vertex_coords"] = "#define VERTEX_WORLD_COORDS_USED\n";
    spatial_rendermode_defs["ensure_correct_normals"] = "#define ENSURE_CORRECT_NORMALS\n";
    spatial_rendermode_defs["cull_front"] = "#define DO_SIDE_CHECK\n";
    spatial_rendermode_defs["cull_disabled"] = "#define DO_SIDE_CHECK\n";

    bool force_lambert = GLOBAL_GET("rendering/quality/shading/force_lambert_over_burley").as<bool>();

    if (!force_lambert) {
        spatial_rendermode_defs["diffuse_burley"] = "#define DIFFUSE_BURLEY\n";
    }

    spatial_rendermode_defs["diffuse_oren_nayar"] = "#define DIFFUSE_OREN_NAYAR\n";
    spatial_rendermode_defs["diffuse_lambert_wrap"] = "#define DIFFUSE_LAMBERT_WRAP\n";
    spatial_rendermode_defs["diffuse_toon"] = "#define DIFFUSE_TOON\n";

    bool force_blinn = GLOBAL_GET("rendering/quality/shading/force_blinn_over_ggx").as<bool>();

    if (!force_blinn) {
        spatial_rendermode_defs["specular_schlick_ggx"] = "#define SPECULAR_SCHLICK_GGX\n";
    } else {
        spatial_rendermode_defs["specular_schlick_ggx"] = "#define SPECULAR_BLINN\n";
    }

    spatial_rendermode_defs["specular_blinn"] = "#define SPECULAR_BLINN\n";
    spatial_rendermode_defs["specular_phong"] = "#define SPECULAR_PHONG\n";
    spatial_rendermode_defs["specular_toon"] = "#define SPECULAR_TOON\n";
    spatial_rendermode_defs["specular_disabled"] = "#define SPECULAR_DISABLED\n";
    spatial_rendermode_defs["shadows_disabled"] = "#define SHADOWS_DISABLED\n";
    spatial_rendermode_defs["ambient_light_disabled"] = "#define AMBIENT_LIGHT_DISABLED\n";
    spatial_rendermode_defs["shadow_to_opacity"] = "#define USE_SHADOW_TO_OPACITY\n";

    /* PARTICLES SHADER */
    auto &particle_renames(actions[(int)RS::ShaderMode::PARTICLES].renames);
    auto &particle_rendermode_defs(actions[(int)RS::ShaderMode::PARTICLES].render_mode_defines);

    particle_renames["COLOR"] = "out_color";
    particle_renames["VELOCITY"] = "out_velocity_active.xyz";
    particle_renames["MASS"] = "mass";
    particle_renames["ACTIVE"] = "shader_active";
    particle_renames["RESTART"] = "restart";
    particle_renames["CUSTOM"] = "out_custom";
    particle_renames["TRANSFORM"] = "xform";
    particle_renames["TIME"] = "time";
    particle_renames["LIFETIME"] = "lifetime";
    particle_renames["DELTA"] = "local_delta";
    particle_renames["NUMBER"] = "particle_number";
    particle_renames["INDEX"] = "index";
    particle_renames["GRAVITY"] = "current_gravity";
    particle_renames["EMISSION_TRANSFORM"] = "emission_transform";
    particle_renames["RANDOM_SEED"] = "random_seed";

    particle_rendermode_defs["disable_force"] = "#define DISABLE_FORCE\n";
    particle_rendermode_defs["disable_velocity"] = "#define DISABLE_VELOCITY\n";
    particle_rendermode_defs["keep_data"] = "#define ENABLE_KEEP_DATA\n";

    vertex_name = "vertex";
    fragment_name = "fragment";
    light_name = "light";
    time_name = "TIME";

    Vector<String> func_list;

    ShaderLanguage::get_builtin_funcs(&func_list);

    for (const String &E : func_list) {
        internal_functions.emplace(E);
    }
}
