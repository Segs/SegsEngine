/*************************************************************************/
/*  shader_gles3.h                                                       */
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

#include "core/hashfuncs.h"
#include "core/hash_map.h"
#include "core/map.h"
#include "core/math/camera_matrix.h"
#include "core/color.h"
#include "core/variant.h"
#include "core/se_string.h"
#include "core/vector.h"
#include "core/set.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/face3.h"
#include "core/math/plane.h"
#include "core/math/quat.h"
#include "core/math/transform.h"
#include "core/math/transform_2d.h"
#include "core/math/vector3.h"

#include "platform_config.h"
#include "thirdparty/glad/glad/glad.h"

#include <cstdio>

union ShaderVersionKey {

    struct {
        uint32_t version;
        uint32_t code_version;
    };
    uint64_t key;
    bool operator==(const ShaderVersionKey &p_key) const { return key == p_key.key; }
    bool operator<(const ShaderVersionKey &p_key) const { return key < p_key.key; }
};
template<>
struct Hasher<ShaderVersionKey> {

    uint32_t operator()(const ShaderVersionKey &p_key) { return Hasher<uint64_t>()(p_key.key); }
};

class ShaderGLES3 {
protected:
    struct Enum {

        uint64_t mask;
        uint64_t shift;
        const char *defines[16];
    };

    struct EnumValue {

        uint64_t set_mask;
        uint64_t clear_mask;
    };

    struct AttributePair {

        const char *name;
        int index;
    };

    struct UniformPair {
        const char *name;
        VariantType type_hint;
    };

    struct TexUnitPair {

        const char *name;
        int index;
    };

    struct UBOPair {

        const char *name;
        int index;
    };

    struct Feedback {

        const char *name;
        int conditional;
    };

    bool uniforms_dirty;

private:
    //@TODO Optimize to a fixed set of shader pools and use a LRU
    int uniform_count;
    int texunit_pair_count;
    int conditional_count;
    int ubo_count;
    int feedback_count;
    int vertex_code_start;
    int fragment_code_start;
    int attribute_pair_count;

    struct CustomCode {

        se_string vertex;
        se_string vertex_globals;
        se_string fragment;
        se_string fragment_globals;
        se_string light;
        se_string uniforms;
        uint32_t version;
        Vector<StringName> texture_uniforms;
        Vector<se_string> custom_defines;
        Set<uint32_t> versions;
    };

    struct Version {
        GLuint id=0;
        GLuint vert_id=0;
        GLuint frag_id=0;
        GLint *uniform_location=nullptr;
        Vector<GLint> texture_uniform_locations;
        uint32_t code_version=0;
        bool ok=false;
    };

    Version *version;



    //this should use a way more cachefriendly version..
    HashMap<ShaderVersionKey, Version> version_map;

    HashMap<uint32_t, CustomCode> custom_code_map;
    uint32_t last_custom_code;

    ShaderVersionKey conditional_version;
    ShaderVersionKey new_conditional_version;

    virtual const char *get_shader_name() const = 0;

    const char **conditional_defines;
    const char **uniform_names;
    const AttributePair *attribute_pairs;
    const TexUnitPair *texunit_pairs;
    const UBOPair *ubo_pairs;
    const Feedback *feedbacks;
    const char *vertex_code;
    const char *fragment_code;
    se_string fragment_code0;
    se_string fragment_code1;
    se_string fragment_code2;
    se_string fragment_code3;
    se_string fragment_code4;

    se_string vertex_code0;
    se_string vertex_code1;
    se_string vertex_code2;
    se_string vertex_code3;

    PODVector<se_string> custom_defines;

    int base_material_tex_index;

    Version *get_current_version();

    static ShaderGLES3 *active;

    int max_image_units;

    void _set_uniform_variant(GLint p_uniform, const Variant &p_value) {

        if (p_uniform < 0)
            return; // do none
        switch (p_value.get_type()) {

            case VariantType::BOOL:
            case VariantType::INT: {

                int val = p_value;
                glUniform1i(p_uniform, val);
            } break;
            case VariantType::REAL: {

                real_t val = p_value;
                glUniform1f(p_uniform, val);
            } break;
            case VariantType::COLOR: {

                Color val = p_value;
                glUniform4f(p_uniform, val.r, val.g, val.b, val.a);
            } break;
            case VariantType::VECTOR2: {

                Vector2 val = p_value;
                glUniform2f(p_uniform, val.x, val.y);
            } break;
            case VariantType::VECTOR3: {

                Vector3 val = p_value;
                glUniform3f(p_uniform, val.x, val.y, val.z);
            } break;
            case VariantType::PLANE: {

                Plane val = p_value;
                glUniform4f(p_uniform, val.normal.x, val.normal.y, val.normal.z, val.d);
            } break;
            case VariantType::QUAT: {

                Quat val = p_value;
                glUniform4f(p_uniform, val.x, val.y, val.z, val.w);
            } break;

            case VariantType::TRANSFORM2D: {

                Transform2D tr = p_value;
                GLfloat matrix[16] = { /* build a 16x16 matrix */
                    tr.elements[0][0],
                    tr.elements[0][1],
                    0,
                    0,
                    tr.elements[1][0],
                    tr.elements[1][1],
                    0,
                    0,
                    0,
                    0,
                    1,
                    0,
                    tr.elements[2][0],
                    tr.elements[2][1],
                    0,
                    1
                };

                glUniformMatrix4fv(p_uniform, 1, false, matrix);

            } break;
            case VariantType::BASIS:
            case VariantType::TRANSFORM: {

                Transform tr = p_value;
                GLfloat matrix[16] = { /* build a 16x16 matrix */
                    tr.basis.elements[0][0],
                    tr.basis.elements[1][0],
                    tr.basis.elements[2][0],
                    0,
                    tr.basis.elements[0][1],
                    tr.basis.elements[1][1],
                    tr.basis.elements[2][1],
                    0,
                    tr.basis.elements[0][2],
                    tr.basis.elements[1][2],
                    tr.basis.elements[2][2],
                    0,
                    tr.origin.x,
                    tr.origin.y,
                    tr.origin.z,
                    1
                };

                glUniformMatrix4fv(p_uniform, 1, false, matrix);
            } break;
            default: {
                ERR_FAIL()
            } // do nothing
        }
    }

    Map<uint32_t, Variant> uniform_defaults;
    Map<uint32_t, CameraMatrix> uniform_cameras;

protected:
    _FORCE_INLINE_ int _get_uniform(int p_which) const;
    _FORCE_INLINE_ void _set_conditional(int p_which, bool p_value);

    void setup(const char **p_conditional_defines, int p_conditional_count, const char **p_uniform_names, int p_uniform_count, const AttributePair *p_attribute_pairs, int p_attribute_count, const TexUnitPair *p_texunit_pairs, int p_texunit_pair_count, const UBOPair *p_ubo_pairs, int p_ubo_pair_count, const Feedback *p_feedback, int p_feedback_count, const char *p_vertex_code, const char *p_fragment_code, int p_vertex_code_start, int p_fragment_code_start);

    ShaderGLES3();

public:
    enum {
        CUSTOM_SHADER_DISABLED = 0
    };

    GLint get_uniform_location(se_string_view p_name) const;
    GLint get_uniform_location(int p_index) const;

    static _FORCE_INLINE_ ShaderGLES3 *get_active() { return active; }
    bool bind();
    void unbind();
    void bind_uniforms();

    inline GLuint get_program() const { return version ? version->id : 0; }

    void clear_caches();

    uint32_t create_custom_shader();
    void set_custom_shader_code(uint32_t p_code_id, const se_string &p_vertex, const se_string &p_vertex_globals,
            const se_string &p_fragment, const se_string &p_light, const se_string &p_fragment_globals, const se_string &p_uniforms,
            const Vector<StringName> &p_texture_uniforms, const Vector<se_string> &p_custom_defines);
    void set_custom_shader(uint32_t p_code_id);
    void free_custom_shader(uint32_t p_code_id);

    void set_uniform_default(int p_idx, const Variant &p_value) {

        if (p_value.get_type() == VariantType::NIL) {

            uniform_defaults.erase(p_idx);
        } else {

            uniform_defaults[p_idx] = p_value;
        }
        uniforms_dirty = true;
    }

    uint32_t get_version() const { return new_conditional_version.version; }
    _FORCE_INLINE_ bool is_version_valid() const { return version && version->ok; }

    void set_uniform_camera(int p_idx, const CameraMatrix &p_mat) {

        uniform_cameras[p_idx] = p_mat;
        uniforms_dirty = true;
    };

    _FORCE_INLINE_ void set_texture_uniform(int p_idx, const Variant &p_value) {

        ERR_FAIL_COND(!version)
        ERR_FAIL_INDEX(p_idx, version->texture_uniform_locations.size());
        _set_uniform_variant(version->texture_uniform_locations[p_idx], p_value);
    }

    _FORCE_INLINE_ GLint get_texture_uniform_location(int p_idx) {

        ERR_FAIL_COND_V(!version, -1)
        ERR_FAIL_INDEX_V(p_idx, version->texture_uniform_locations.size(), -1);
        return version->texture_uniform_locations[p_idx];
    }

    virtual void init() = 0;
    void finish();

    void set_base_material_tex_index(int p_idx);

    void add_custom_define(const se_string &p_define) {
        custom_defines.emplace_back(p_define);
    }

    virtual ~ShaderGLES3();
};

// called a lot, made inline

int ShaderGLES3::_get_uniform(int p_which) const {

    ERR_FAIL_INDEX_V(p_which, uniform_count, -1);
    ERR_FAIL_COND_V(!version, -1)
    return version->uniform_location[p_which];
}

void ShaderGLES3::_set_conditional(int p_which, bool p_value) {

    ERR_FAIL_INDEX(p_which, conditional_count)
    if (p_value)
        new_conditional_version.version |= (1 << p_which);
    else
        new_conditional_version.version &= ~(1 << p_which);
}
