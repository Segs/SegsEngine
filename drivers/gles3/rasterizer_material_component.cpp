#include "rasterizer_material_component.h"

#include "drivers/gles3/rasterizer_common_geometry_component.h"
#include "rasterizer_material_component.h"

#include "rasterizer_shader_component.h"
#include "rasterizer_storage_gles3.h"
#include "rasterizer_surface_component.h"
#include "servers/rendering/render_entity_getter.h"

struct MaterialDirtyMarker {

};
static void _update_material(RasterizerGLES3ShadersStorage &shaders, RasterizerMaterialComponent *material);

void _material_make_dirty(const RasterizerMaterialComponent *p_material) {

    VSG::ecs->registry.emplace_or_replace<MaterialDirtyMarker>(p_material->self);
}

RenderingEntity RasterizerStorageGLES3::material_create() {
    auto res=VSG::ecs->create();
    VSG::ecs->registry.emplace<RasterizerMaterialComponent>(res).self = res;
    return res;
}


static void _fill_std140_variant_ubo_value(ShaderLanguage::DataType type, const Variant &value, uint8_t *data, bool p_linear_color) {
    switch (type) {
        case ShaderLanguage::TYPE_BOOL: {

            bool v = value.as<bool>();

            GLuint *gui = (GLuint *)data;
            *gui = v ? GL_TRUE : GL_FALSE;
        } break;
        case ShaderLanguage::TYPE_BVEC2: {

            int v = value.as<int>();
            GLuint *gui = (GLuint *)data;
            gui[0] = (v & 1) ? GL_TRUE : GL_FALSE;
            gui[1] = (v & 2) ? GL_TRUE : GL_FALSE;

        } break;
        case ShaderLanguage::TYPE_BVEC3: {

            int v = value.as<int>();
            GLuint *gui = (GLuint *)data;
            gui[0] = (v & 1) ? GL_TRUE : GL_FALSE;
            gui[1] = (v & 2) ? GL_TRUE : GL_FALSE;
            gui[2] = (v & 4) ? GL_TRUE : GL_FALSE;

        } break;
        case ShaderLanguage::TYPE_BVEC4: {

            int v = value.as<int>();
            GLuint *gui = (GLuint *)data;
            gui[0] = (v & 1) ? GL_TRUE : GL_FALSE;
            gui[1] = (v & 2) ? GL_TRUE : GL_FALSE;
            gui[2] = (v & 4) ? GL_TRUE : GL_FALSE;
            gui[3] = (v & 8) ? GL_TRUE : GL_FALSE;

        } break;
        case ShaderLanguage::TYPE_INT: {

            int v = value.as<int>();
            GLint *gui = (GLint *)data;
            gui[0] = v;

        } break;
        case ShaderLanguage::TYPE_IVEC2: {

            PoolVector<int> iv = value.as<PoolVector<int>>();
            int s = iv.size();
            GLint *gui = (GLint *)data;

            PoolVector<int>::Read r = iv.read();

            for (int i = 0; i < 2; i++) {
                if (i < s) {
                    gui[i] = r[i];
                } else {
                    gui[i] = 0;
                }
            }

        } break;
        case ShaderLanguage::TYPE_IVEC3: {

            PoolVector<int> iv = value.as<PoolVector<int>>();
            int s = iv.size();
            GLint *gui = (GLint *)data;

            PoolVector<int>::Read r = iv.read();

            for (uint32_t i = 0; i < 3; i++) {
                if (i < s)
                    gui[i] = r[i];
                else
                    gui[i] = 0;
            }
        } break;
        case ShaderLanguage::TYPE_IVEC4: {

            PoolVector<int> iv = value.as<PoolVector<int>>();
            int s = iv.size();
            GLint *gui = (GLint *)data;

            PoolVector<int>::Read r = iv.read();

            for (int i = 0; i < 4; i++) {
                if (i < s)
                    gui[i] = r[i];
                else
                    gui[i] = 0;
            }
        } break;
        case ShaderLanguage::TYPE_UINT: {

            int v = value.as<int>();
            GLuint *gui = (GLuint *)data;
            gui[0] = v;

        } break;
        case ShaderLanguage::TYPE_UVEC2: {

            PoolVector<int> iv = value.as<PoolVector<int>>();
            int s = iv.size();
            GLuint *gui = (GLuint *)data;

            PoolVector<int>::Read r = iv.read();

            for (int i = 0; i < 2; i++) {
                if (i < s)
                    gui[i] = r[i];
                else
                    gui[i] = 0;
            }
        } break;
        case ShaderLanguage::TYPE_UVEC3: {
            PoolVector<int> iv = value.as<PoolVector<int>>();
            int s = iv.size();
            GLuint *gui = (GLuint *)data;

            PoolVector<int>::Read r = iv.read();

            for (int i = 0; i < 3; i++) {
                if (i < s)
                    gui[i] = r[i];
                else
                    gui[i] = 0;
            }

        } break;
        case ShaderLanguage::TYPE_UVEC4: {
            PoolVector<int> iv = value.as<PoolVector<int>>();
            int s = iv.size();
            GLuint *gui = (GLuint *)data;

            PoolVector<int>::Read r = iv.read();

            for (int i = 0; i < 4; i++) {
                if (i < s)
                    gui[i] = r[i];
                else
                    gui[i] = 0;
            }
        } break;
        case ShaderLanguage::TYPE_FLOAT: {
            float v = value.as<float>();
            GLfloat *gui = (GLfloat *)data;
            gui[0] = v;

        } break;
        case ShaderLanguage::TYPE_VEC2: {
            Vector2 v = value.as<Vector2>();
            GLfloat *gui = (GLfloat *)data;
            gui[0] = v.x;
            gui[1] = v.y;

        } break;
        case ShaderLanguage::TYPE_VEC3: {
            Vector3 v = value.as<Vector3>();
            GLfloat *gui = (GLfloat *)data;
            gui[0] = v.x;
            gui[1] = v.y;
            gui[2] = v.z;

        } break;
        case ShaderLanguage::TYPE_VEC4: {

            GLfloat *gui = (GLfloat *)data;

            if (value.get_type() == VariantType::COLOR) {
                Color v = value.as<Color>();

                if (p_linear_color) {
                    v = v.to_linear();
                }

                gui[0] = v.r;
                gui[1] = v.g;
                gui[2] = v.b;
                gui[3] = v.a;
            } else if (value.get_type() == VariantType::RECT2) {
                Rect2 v = value.as<Rect2>();

                gui[0] = v.position.x;
                gui[1] = v.position.y;
                gui[2] = v.size.x;
                gui[3] = v.size.y;
            } else if (value.get_type() == VariantType::QUAT) {
                Quat v = value.as<Quat>();

                gui[0] = v.x;
                gui[1] = v.y;
                gui[2] = v.z;
                gui[3] = v.w;
            } else {
                Plane v = value.as<Plane>();

                gui[0] = v.normal.x;
                gui[1] = v.normal.y;
                gui[2] = v.normal.z;
                gui[3] = v.d;
            }
        } break;
        case ShaderLanguage::TYPE_MAT2: {
            Transform2D v = value.as<Transform2D>();
            GLfloat *gui = (GLfloat *)data;

                 //in std140 members of mat2 are treated as vec4s
            gui[0] = v.elements[0][0];
            gui[1] = v.elements[0][1];
            gui[2] = 0;
            gui[3] = 0;
            gui[4] = v.elements[1][0];
            gui[5] = v.elements[1][1];
            gui[6] = 0;
            gui[7] = 0;
        } break;
        case ShaderLanguage::TYPE_MAT3: {

            Basis v = value.as<Basis>();
            GLfloat *gui = (GLfloat *)data;

            gui[0] = v.elements[0][0];
            gui[1] = v.elements[1][0];
            gui[2] = v.elements[2][0];
            gui[3] = 0;
            gui[4] = v.elements[0][1];
            gui[5] = v.elements[1][1];
            gui[6] = v.elements[2][1];
            gui[7] = 0;
            gui[8] = v.elements[0][2];
            gui[9] = v.elements[1][2];
            gui[10] = v.elements[2][2];
            gui[11] = 0;
        } break;
        case ShaderLanguage::TYPE_MAT4: {

            Transform v = value.as<Transform>();
            GLfloat *gui = (GLfloat *)data;

            gui[0] = v.basis.elements[0][0];
            gui[1] = v.basis.elements[1][0];
            gui[2] = v.basis.elements[2][0];
            gui[3] = 0;
            gui[4] = v.basis.elements[0][1];
            gui[5] = v.basis.elements[1][1];
            gui[6] = v.basis.elements[2][1];
            gui[7] = 0;
            gui[8] = v.basis.elements[0][2];
            gui[9] = v.basis.elements[1][2];
            gui[10] = v.basis.elements[2][2];
            gui[11] = 0;
            gui[12] = v.origin.x;
            gui[13] = v.origin.y;
            gui[14] = v.origin.z;
            gui[15] = 1;
        } break;
        default: {
        }
    }
}

static void _fill_std140_ubo_value(ShaderLanguage::DataType type, const Vector<ShaderLanguage::ConstantNode::Value> &value, uint8_t *data) {

    switch (type) {
        case ShaderLanguage::TYPE_BOOL: {

            GLuint *gui = (GLuint *)data;
            *gui = value[0].boolean ? GL_TRUE : GL_FALSE;
        } break;
        case ShaderLanguage::TYPE_BVEC2: {

            GLuint *gui = (GLuint *)data;
            gui[0] = value[0].boolean ? GL_TRUE : GL_FALSE;
            gui[1] = value[1].boolean ? GL_TRUE : GL_FALSE;

        } break;
        case ShaderLanguage::TYPE_BVEC3: {

            GLuint *gui = (GLuint *)data;
            gui[0] = value[0].boolean ? GL_TRUE : GL_FALSE;
            gui[1] = value[1].boolean ? GL_TRUE : GL_FALSE;
            gui[2] = value[2].boolean ? GL_TRUE : GL_FALSE;

        } break;
        case ShaderLanguage::TYPE_BVEC4: {

            GLuint *gui = (GLuint *)data;
            gui[0] = value[0].boolean ? GL_TRUE : GL_FALSE;
            gui[1] = value[1].boolean ? GL_TRUE : GL_FALSE;
            gui[2] = value[2].boolean ? GL_TRUE : GL_FALSE;
            gui[3] = value[3].boolean ? GL_TRUE : GL_FALSE;

        } break;
        case ShaderLanguage::TYPE_INT: {

            GLint *gui = (GLint *)data;
            gui[0] = value[0].sint;

        } break;
        case ShaderLanguage::TYPE_IVEC2: {

            GLint *gui = (GLint *)data;

            for (int i = 0; i < 2; i++) {
                gui[i] = value[i].sint;
            }

        } break;
        case ShaderLanguage::TYPE_IVEC3: {

            GLint *gui = (GLint *)data;

            for (int i = 0; i < 3; i++) {
                gui[i] = value[i].sint;
            }

        } break;
        case ShaderLanguage::TYPE_IVEC4: {

            GLint *gui = (GLint *)data;

            for (int i = 0; i < 4; i++) {
                gui[i] = value[i].sint;
            }

        } break;
        case ShaderLanguage::TYPE_UINT: {

            GLuint *gui = (GLuint *)data;
            gui[0] = value[0].uint;

        } break;
        case ShaderLanguage::TYPE_UVEC2: {

            GLint *gui = (GLint *)data;

            for (int i = 0; i < 2; i++) {
                gui[i] = value[i].uint;
            }
        } break;
        case ShaderLanguage::TYPE_UVEC3: {
            GLint *gui = (GLint *)data;

            for (int i = 0; i < 3; i++) {
                gui[i] = value[i].uint;
            }

        } break;
        case ShaderLanguage::TYPE_UVEC4: {
            GLint *gui = (GLint *)data;

            for (int i = 0; i < 4; i++) {
                gui[i] = value[i].uint;
            }
        } break;
        case ShaderLanguage::TYPE_FLOAT: {

            GLfloat *gui = (GLfloat *)data;
            gui[0] = value[0].real;

        } break;
        case ShaderLanguage::TYPE_VEC2: {

            GLfloat *gui = (GLfloat *)data;

            for (int i = 0; i < 2; i++) {
                gui[i] = value[i].real;
            }

        } break;
        case ShaderLanguage::TYPE_VEC3: {

            GLfloat *gui = (GLfloat *)data;

            for (int i = 0; i < 3; i++) {
                gui[i] = value[i].real;
            }

        } break;
        case ShaderLanguage::TYPE_VEC4: {

            GLfloat *gui = (GLfloat *)data;

            for (int i = 0; i < 4; i++) {
                gui[i] = value[i].real;
            }
        } break;
        case ShaderLanguage::TYPE_MAT2: {
            GLfloat *gui = (GLfloat *)data;

                 //in std140 members of mat2 are treated as vec4s
            gui[0] = value[0].real;
            gui[1] = value[1].real;
            gui[2] = 0;
            gui[3] = 0;
            gui[4] = value[2].real;
            gui[5] = value[3].real;
            gui[6] = 0;
            gui[7] = 0;
        } break;
        case ShaderLanguage::TYPE_MAT3: {

            GLfloat *gui = (GLfloat *)data;

            gui[0] = value[0].real;
            gui[1] = value[1].real;
            gui[2] = value[2].real;
            gui[3] = 0;
            gui[4] = value[3].real;
            gui[5] = value[4].real;
            gui[6] = value[5].real;
            gui[7] = 0;
            gui[8] = value[6].real;
            gui[9] = value[7].real;
            gui[10] = value[8].real;
            gui[11] = 0;
        } break;
        case ShaderLanguage::TYPE_MAT4: {

            GLfloat *gui = (GLfloat *)data;

            for (int i = 0; i < 16; i++) {
                gui[i] = value[i].real;
            }
        } break;
        default: {
        }
    }
}

static void _fill_std140_ubo_empty(ShaderLanguage::DataType type, uint8_t *data) {

    switch (type) {

        case ShaderLanguage::TYPE_BOOL:
        case ShaderLanguage::TYPE_INT:
        case ShaderLanguage::TYPE_UINT:
        case ShaderLanguage::TYPE_FLOAT: {
            memset(data, 0, 4);
        } break;
        case ShaderLanguage::TYPE_BVEC2:
        case ShaderLanguage::TYPE_IVEC2:
        case ShaderLanguage::TYPE_UVEC2:
        case ShaderLanguage::TYPE_VEC2: {
            memset(data, 0, 8);
        } break;
        case ShaderLanguage::TYPE_BVEC3:
        case ShaderLanguage::TYPE_IVEC3:
        case ShaderLanguage::TYPE_UVEC3:
        case ShaderLanguage::TYPE_VEC3: {
            memset(data,0, 12);
        } break;
        case ShaderLanguage::TYPE_BVEC4:
        case ShaderLanguage::TYPE_IVEC4:
        case ShaderLanguage::TYPE_UVEC4:
        case ShaderLanguage::TYPE_VEC4: {

            memset(data, 0, 16);
        } break;
        case ShaderLanguage::TYPE_MAT2: {

            memset(data, 0, 32);
        } break;
        case ShaderLanguage::TYPE_MAT3: {

            memset(data, 0, 48);
        } break;
        case ShaderLanguage::TYPE_MAT4: {
            memset(data, 0, 64);
        } break;

        default: {
        }
    }
}

static void _update_material(RasterizerGLES3ShadersStorage &shaders, RasterizerMaterialComponent *material) {

    auto *shader = get<RasterizerShaderComponent>(material->shader);

    if (shader && VSG::ecs->registry.any_of<ShaderDirtyMarker>(material->shader)) {
        _update_shader(shaders,shader);
    }

    if (!shader || !shader->valid) {
        return;
    }

    //update caches

    {
        bool can_cast_shadow = false;
        bool is_animated = false;

        if (shader && shader->mode == RS::ShaderMode::SPATIAL) {

            if (shader->spatial.blend_mode == RasterizerShaderComponent::Node3D::BLEND_MODE_MIX &&
                    (!shader->spatial.uses_alpha || shader->spatial.depth_draw_mode == RasterizerShaderComponent::Node3D::DEPTH_DRAW_ALPHA_PREPASS)) {
                can_cast_shadow = true;
            }

            if (shader->spatial.uses_discard && shader->uses_fragment_time) {
                is_animated = true;
            }

            if (shader->spatial.uses_vertex && shader->uses_vertex_time) {
                is_animated = true;
            }

            if (can_cast_shadow != material->can_cast_shadow_cache || is_animated != material->is_animated_cache) {
                material->can_cast_shadow_cache = can_cast_shadow;
                material->is_animated_cache = is_animated;

                for (eastl::pair<RenderingEntity,int> E : material->geometry_owners) {
                    auto *surf = get<RasterizerSurfaceComponent>(E.first);
                    if(surf)
                        surf->material_changed_notify();
                }

                for (eastl::pair<RenderingEntity,int> E : material->instance_owners) {
                    auto &ic = VSG::ecs->registry.get<RenderingInstanceComponent>(E.first);
                    ic.base_changed(false, true);
                }
            }
        }
    }

         //clear ubo if it needs to be cleared
    if (material->ubo_size) {

        assert(shader==get<RasterizerShaderComponent>(material->shader));
        if (!shader || shader->ubo_size != material->ubo_size) {
            //bye bye ubo
            material->ubo_id.release();
            material->ubo_size = 0;
        }
    }

         //create ubo if it needs to be created
    if (material->ubo_size == 0 && shader && shader->ubo_size) {

        material->ubo_id.create();
        glBindBuffer(GL_UNIFORM_BUFFER, material->ubo_id);
        glBufferData(GL_UNIFORM_BUFFER, shader->ubo_size, nullptr, GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        material->ubo_size = shader->ubo_size;
    }

         //fill up the UBO if it needs to be filled
    if (shader && material->ubo_size) {
        uint8_t *local_ubo = (uint8_t *)alloca(material->ubo_size);
        //TODO: in debug mode non-set bytes of the ubo are filled with garbage, maybe it's ok?
        for (eastl::pair<const StringName,ShaderLanguage::ShaderNode::Uniform> &E : shader->uniforms) {

            if (E.second.order < 0) {
                continue; // texture, does not go here
            }

                 //regular uniform
            uint8_t *data = &local_ubo[shader->ubo_offsets[E.second.order]];

            auto V = material->params.find(E.first);

            if (V!=material->params.end()) {
                //user provided
                _fill_std140_variant_ubo_value(E.second.type, V->second, data, shader->mode == RS::ShaderMode::SPATIAL);

            } else if (!E.second.default_value.empty()) {
                //default value
                _fill_std140_ubo_value(E.second.type, E.second.default_value, data);
                //value=E.second.default_value;
            } else {
                //zero because it was not provided
                if (E.second.type == ShaderLanguage::TYPE_VEC4 && E.second.hint == ShaderLanguage::ShaderNode::Uniform::HINT_COLOR) {
                    //colors must be set as black, with alpha as 1.0
                    _fill_std140_variant_ubo_value(E.second.type, Color(0, 0, 0, 1), data, shader->mode == RS::ShaderMode::SPATIAL);
                } else {
                    //else just zero it out
                    _fill_std140_ubo_empty(E.second.type, data);
                }
            }
        }

        glBindBuffer(GL_UNIFORM_BUFFER, material->ubo_id);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, material->ubo_size, local_ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
    // if no shader, or shader without textures, cleanup the material texture arrays.
    if (!shader || !shader->texture_count) {
        material->textures.clear();
        material->texture_is_3d.clear();
        return;
    }
    // set up the texture array, for easy access when it needs to be drawn

    material->texture_is_3d.resize(shader->texture_count);
    material->textures.resize(shader->texture_count,entt::null);

         // Update the material's texture array.
    for (eastl::pair<const StringName,ShaderLanguage::ShaderNode::Uniform> &E : shader->uniforms) {

        if (E.second.texture_order < 0)
            continue; // not a texture, does not go here

        RenderingEntity texture=entt::null;

        switch (E.second.type) {
            case ShaderLanguage::TYPE_SAMPLER3D:
            case ShaderLanguage::TYPE_SAMPLER2DARRAY: {
                material->texture_is_3d[E.second.texture_order] = true;
            } break;
            default: {
                material->texture_is_3d[E.second.texture_order] = false;
            } break;
        }

        auto V = material->params.find(E.first);
        if (V!=material->params.end()) {
            texture = V->second.as<RenderingEntity>();
        }

        if (texture==entt::null) {
            auto W = shader->default_textures.find(E.first);
            if (W!=shader->default_textures.end()) {
                texture = W->second;
            }
        }
        assert(texture==entt::null || VSG::ecs->registry.valid(texture));
        material->textures[E.second.texture_order] = texture;
    }
}

void RasterizerStorageGLES3::material_set_shader(RenderingEntity p_material, RenderingEntity p_shader) {

    auto *material = get<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND(!material);

    auto *shader = get<RasterizerShaderComponent>(p_shader);
    auto *current_shader = get<RasterizerShaderComponent>(material->shader);

    if (current_shader) {
        //if we have a shader assigned, remove ourselves from its shader material list
        current_shader->materials.erase_first_unsorted(p_material);
    }
    material->shader = p_shader;

    if (shader) {
        shader->materials.emplace_back(p_material);
    }

    _material_make_dirty(material);
}

RenderingEntity RasterizerStorageGLES3::material_get_shader(RenderingEntity p_material) const {

    const RasterizerMaterialComponent *material = getUnchecked<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND_V(!material, entt::null);

    return material->shader;
}

void RasterizerStorageGLES3::material_set_param(RenderingEntity p_material, const StringName &p_param, const Variant &p_value) {

    RasterizerMaterialComponent *material = getUnchecked<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND(!material);

    if (p_value.get_type() == VariantType::NIL)
        material->params.erase(p_param);
    else {
        if(p_value.get_type()==VariantType::REN_ENT) {
            auto v = p_value.as<RenderingEntity>();
            assert(v==entt::null || VSG::ecs->registry.valid(v));
        }
        material->params[p_param] = p_value;
    }

    _material_make_dirty(material);
}
Variant RasterizerStorageGLES3::material_get_param(RenderingEntity p_material, const StringName &p_param) const {

    const RasterizerMaterialComponent *material = getUnchecked<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND_V(!material, Variant());

    if (material->params.contains(p_param))
        return material->params.at(p_param);

    return material_get_param_default(p_material, p_param);
}

Variant RasterizerStorageGLES3::material_get_param_default(RenderingEntity p_material, const StringName &p_param) const {
    const auto *material = getUnchecked<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND_V(!material, Variant());

    if (material->shader!=entt::null) {
        auto *shader=getUnchecked<RasterizerShaderComponent>(material->shader);
        if (shader->uniforms.contains(p_param)) {
            ShaderLanguage::ShaderNode::Uniform uniform = shader->uniforms[p_param];
            Vector<ShaderLanguage::ConstantNode::Value> default_value = uniform.default_value;
            return ShaderLanguage::constant_value_to_variant(default_value, uniform.type, uniform.hint);
        }
    }
    return Variant();
}

void RasterizerStorageGLES3::material_set_line_width(RenderingEntity p_material, float p_width) {

    auto *material = getUnchecked<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND(!material);

    material->line_width = p_width;
}

void RasterizerStorageGLES3::material_set_next_pass(RenderingEntity p_material, RenderingEntity p_next_material) {

    auto *material = getUnchecked<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND(!material);

    material->next_pass = p_next_material;
}

bool RasterizerStorageGLES3::material_is_animated(RenderingEntity p_material) {

    auto *material = getUnchecked<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND_V(!material, false);

    if (VSG::ecs->registry.any_of<MaterialDirtyMarker>(material->self)) {
        _update_material(shaders,material);
        VSG::ecs->registry.erase<MaterialDirtyMarker>(material->self);
    }

    bool animated = material->is_animated_cache;
    if (!animated && material->next_pass!=entt::null) {
        animated = material_is_animated(material->next_pass);
    }
    return animated;
}

bool RasterizerStorageGLES3::material_casts_shadows(RenderingEntity p_material) {

    RasterizerMaterialComponent *material = get<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND_V(!material, false);
    if (VSG::ecs->registry.any_of<MaterialDirtyMarker>(material->self)) {
        _update_material(shaders,material);
        VSG::ecs->registry.erase<MaterialDirtyMarker>(material->self);
    }

    bool casts_shadows = material->can_cast_shadow_cache;

    if (!casts_shadows && material->next_pass!=entt::null) {
        casts_shadows = material_casts_shadows(material->next_pass);
    }

    return casts_shadows;
}

bool RasterizerStorageGLES3::material_uses_tangents(RenderingEntity p_material) {
    RasterizerMaterialComponent *material = get<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND_V(!material, false);

    if (material->shader==entt::null) {
        return false;
    }
    auto *current_shader = getUnchecked<RasterizerShaderComponent>(material->shader);
    if (VSG::ecs->registry.any_of<ShaderDirtyMarker>(material->shader)) {
        _update_shader(shaders,current_shader);
    }

    return current_shader->spatial.uses_tangent;
}

bool RasterizerStorageGLES3::material_uses_ensure_correct_normals(RenderingEntity p_material) {
    RasterizerMaterialComponent *material = get<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND_V(!material, false);

    if (material->shader==entt::null) {
        return false;
    }
    auto *current_shader = getUnchecked<RasterizerShaderComponent>(material->shader);
    if (VSG::ecs->registry.any_of<ShaderDirtyMarker>(material->shader)) {
        _update_shader(shaders,current_shader);
    }

    return current_shader->spatial.uses_ensure_correct_normals;
}

void RasterizerStorageGLES3::material_add_instance_owner(RenderingEntity p_material, RenderingEntity p_instance) {

    RasterizerMaterialComponent *material = get<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND(!material);

    material->instance_owners[p_instance]++;
}

void RasterizerStorageGLES3::material_remove_instance_owner(RenderingEntity p_material, RenderingEntity p_instance) {

    RasterizerMaterialComponent *material = get<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND(!material);

    auto E = material->instance_owners.find(p_instance);
    ERR_FAIL_COND(E==material->instance_owners.end());
    E->second--;

    if (E->second == 0) {
        material->instance_owners.erase(p_instance);
    }
}

void RasterizerStorageGLES3::material_set_render_priority(RenderingEntity p_material, int priority) {

    ERR_FAIL_COND(priority < RS::MATERIAL_RENDER_PRIORITY_MIN);
    ERR_FAIL_COND(priority > RS::MATERIAL_RENDER_PRIORITY_MAX);

    RasterizerMaterialComponent *material = get<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND(!material);

    material->render_priority = priority;
}


void _material_add_geometry(RenderingEntity p_material, RenderingEntity p_geometry) {

    RasterizerMaterialComponent *material = get<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND(!material);
    assert(VSG::ecs->registry.any_of<RasterizerCommonGeometryComponent>(p_geometry));
    material->geometry_owners[p_geometry]++;
}

void material_remove_geometry(RenderingEntity p_material, RenderingEntity p_geometry) {

    RasterizerMaterialComponent *material = get<RasterizerMaterialComponent>(p_material);
    ERR_FAIL_COND(!material);

    auto I = material->geometry_owners.find(p_geometry);
    ERR_FAIL_COND(I==material->geometry_owners.end());

    I->second--;
    if (I->second == 0) {
        material->geometry_owners.erase(I);
    }
}

void RasterizerStorageGLES3::update_dirty_materials() {

    auto view = VSG::ecs->registry.view<MaterialDirtyMarker,RasterizerMaterialComponent>();
    auto sz = VSG::ecs->registry.storage<MaterialDirtyMarker>().size();
    view.each([&](RenderingEntity ent,RasterizerMaterialComponent &mat) {
        _update_material(shaders,&mat);

    });
    assert(sz == VSG::ecs->registry.storage<MaterialDirtyMarker>().size());
    VSG::ecs->registry.clear<MaterialDirtyMarker>();
}

void RasterizerMaterialComponent::release_resources() {

    if(self==entt::null) {
        // moved-from or uninitialized
        return;
    }
    // unregister from shader's user list
    if (shader!=entt::null) {
        auto *current_shader = getUnchecked<RasterizerShaderComponent>(shader);

        current_shader->materials.erase_first_unsorted(self);
        assert(!current_shader->materials.contains(self));
        shader = entt::null;
    }
    ubo_id.release();

    if(!geometry_owners.empty()) {
        auto geom_view(VSG::ecs->registry.view<RasterizerCommonGeometryComponent>());
    // remove from owners
    for (eastl::pair<RenderingEntity,int> E : geometry_owners) {

            assert(geom_view.contains(E.first));
        auto &g = geom_view.get<RasterizerCommonGeometryComponent>(E.first);
        g.material = entt::null;
        }
        geometry_owners.clear();
    }

    if(!instance_owners.empty()) {
    for (eastl::pair<RenderingEntity,int> E : instance_owners) {
        assert(VSG::ecs->registry.any_of<RenderingInstanceComponent>(E.first));
        auto &ins = VSG::ecs->registry.get<RenderingInstanceComponent>(E.first);
        if (ins.material_override == self) {
            ins.material_override = entt::null;
        }
        if (ins.material_overlay == self) {
            ins.material_overlay = entt::null;
        }
        for (auto & rid : ins.materials) {
            if (rid == self) {
                rid = entt::null;
            }
        }
    }
        instance_owners.clear();
    }
}


RasterizerMaterialComponent &RasterizerMaterialComponent::operator=(RasterizerMaterialComponent &&other) {
    release_resources();
    if (this == &other) {
        return *this;
    }
    ubo_id = eastl::move(other.ubo_id);
    shader = eastl::move(other.shader);
    params = eastl::move(other.params);
    geometry_owners = eastl::move(other.geometry_owners);
    instance_owners = eastl::move(other.instance_owners);

    texture_is_3d = eastl::move(other.texture_is_3d);
    textures = eastl::move(other.textures);
    next_pass = eastl::move(other.next_pass);
    self = eastl::move(other.self);
    line_width = other.line_width;
    other.line_width = 0;
    ubo_size = other.ubo_size;
    other.ubo_size = 0;
    render_priority = other.render_priority;
    other.render_priority = 0;

    last_pass=other.last_pass;
    other.last_pass=0;
    index=other.index;
    other.index=0;
    can_cast_shadow_cache=other.can_cast_shadow_cache;
    other.can_cast_shadow_cache=false;
    is_animated_cache=other.is_animated_cache;
    other.is_animated_cache=false;
    return *this;
}

RasterizerMaterialComponent::~RasterizerMaterialComponent() {
    release_resources();
}
