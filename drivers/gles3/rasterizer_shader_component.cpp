#include "rasterizer_shader_component.h"

#include "rasterizer_material_component.h"
#include "rasterizer_canvas_gles3.h"
#include "rasterizer_scene_gles3.h"
#include "rasterizer_storage_gles3.h"
#include "servers/rendering/render_entity_getter.h"

/* SHADER API */


RenderingEntity RasterizerStorageGLES3::shader_create() {

    auto res=VSG::ecs->create();
    RasterizerShaderComponent &shader(VSG::ecs->registry.emplace<RasterizerShaderComponent>(res));
    shader.mode = RS::ShaderMode::SPATIAL;
    shader.shader = &scene->state.scene_shader;
    shader.self = res;
    _shader_make_dirty(&shader);

    return res;
}

void _shader_make_dirty(const RasterizerShaderComponent *p_shader) {
    VSG::ecs->registry.emplace_or_replace<ShaderDirtyMarker>(p_shader->self);
}

void RasterizerStorageGLES3::shader_set_code(RenderingEntity p_shader, const String &p_code) {

    RasterizerShaderComponent *shader = getUnchecked<RasterizerShaderComponent>(p_shader);
    ERR_FAIL_COND(!shader);

    shader->code = p_code;

    String mode_string = ShaderLanguage::get_shader_type(p_code);
    RS::ShaderMode mode;

    if (mode_string == "canvas_item")
        mode = RS::ShaderMode::CANVAS_ITEM;
    else if (mode_string == "particles")
        mode = RS::ShaderMode::PARTICLES;
    else
        mode = RS::ShaderMode::SPATIAL;

    if (shader->custom_code_id && mode != shader->mode) {

        shader->shader->free_custom_shader(shader->custom_code_id);
        shader->custom_code_id = 0;
    }

            shader->mode = mode;

            ShaderGLES3 *shaders[(int)RS::ShaderMode::MAX] = {
                &scene->state.scene_shader,
                &canvas->state.canvas_shader,
                &this->shaders.particles,

                };

    shader->shader = shaders[(int)mode];

    if (shader->custom_code_id == 0) {
        shader->custom_code_id = shader->shader->create_custom_shader();
    }

    _shader_make_dirty(shader);
}
String RasterizerStorageGLES3::shader_get_code(RenderingEntity p_shader) const {

    const RasterizerShaderComponent *shader = getUnchecked<RasterizerShaderComponent>(p_shader);
    ERR_FAIL_COND_V(!shader, String());

    return shader->code;
}

void _update_shader(RasterizerGLES3ShadersStorage &shaders, RasterizerShaderComponent *p_shader) {
    using namespace eastl;
    VSG::ecs->registry.remove<ShaderDirtyMarker>(p_shader->self);

    p_shader->valid = false;
    p_shader->ubo_size = 0;

    p_shader->uniforms.clear();

    if (p_shader->code.empty()) {
        return; //just invalid, but no error
    }

    ShaderCompilerGLES3::GeneratedCode gen_code;
    ShaderCompilerGLES3::IdentifierActions *actions = nullptr;

    int8_t async_mode = (int8_t)ShaderGLES3::ASYNC_MODE_VISIBLE;
    switch (p_shader->mode) {
        case RS::ShaderMode::CANVAS_ITEM: {

            p_shader->canvas_item.light_mode = RasterizerShaderComponent::CanvasItem::LIGHT_MODE_NORMAL;
            p_shader->canvas_item.blend_mode = RasterizerShaderComponent::CanvasItem::BLEND_MODE_MIX;
            p_shader->canvas_item.uses_screen_texture = false;
            p_shader->canvas_item.uses_screen_uv = false;
            p_shader->canvas_item.uses_time = false;
            p_shader->canvas_item.uses_modulate = false;
            p_shader->canvas_item.uses_color = false;
            p_shader->canvas_item.uses_vertex = false;
            p_shader->canvas_item.batch_flags = 0;

            p_shader->canvas_item.uses_world_matrix = false;
            p_shader->canvas_item.uses_extra_matrix = false;
            p_shader->canvas_item.uses_projection_matrix = false;
            p_shader->canvas_item.uses_instance_custom = false;

            shaders.actions_canvas.render_mode_values[StringName("blend_add")] = Pair<int8_t *, int>((int8_t *)&p_shader->canvas_item.blend_mode, RasterizerShaderComponent::CanvasItem::BLEND_MODE_ADD);
            shaders.actions_canvas.render_mode_values["blend_mix"] = Pair<int8_t *, int>((int8_t *)&p_shader->canvas_item.blend_mode, RasterizerShaderComponent::CanvasItem::BLEND_MODE_MIX);
            shaders.actions_canvas.render_mode_values["blend_sub"] = Pair<int8_t *, int>((int8_t *)&p_shader->canvas_item.blend_mode, RasterizerShaderComponent::CanvasItem::BLEND_MODE_SUB);
            shaders.actions_canvas.render_mode_values["blend_mul"] = Pair<int8_t *, int>((int8_t *)&p_shader->canvas_item.blend_mode, RasterizerShaderComponent::CanvasItem::BLEND_MODE_MUL);
            shaders.actions_canvas.render_mode_values["blend_premul_alpha"] = Pair<int8_t *, int>((int8_t *)&p_shader->canvas_item.blend_mode, RasterizerShaderComponent::CanvasItem::BLEND_MODE_PMALPHA);
            shaders.actions_canvas.render_mode_values["blend_disabled"] = Pair<int8_t *, int>((int8_t *)&p_shader->canvas_item.blend_mode, RasterizerShaderComponent::CanvasItem::BLEND_MODE_DISABLED);

            shaders.actions_canvas.render_mode_values["unshaded"] = Pair<int8_t *, int>((int8_t *)&p_shader->canvas_item.light_mode, RasterizerShaderComponent::CanvasItem::LIGHT_MODE_UNSHADED);
            shaders.actions_canvas.render_mode_values["light_only"] = Pair<int8_t *, int>((int8_t *)&p_shader->canvas_item.light_mode, RasterizerShaderComponent::CanvasItem::LIGHT_MODE_LIGHT_ONLY);

            shaders.actions_canvas.usage_flag_pointers["SCREEN_UV"] = &p_shader->canvas_item.uses_screen_uv;
            shaders.actions_canvas.usage_flag_pointers["SCREEN_PIXEL_SIZE"] = &p_shader->canvas_item.uses_screen_uv;
            shaders.actions_canvas.usage_flag_pointers["SCREEN_TEXTURE"] = &p_shader->canvas_item.uses_screen_texture;
            shaders.actions_canvas.usage_flag_pointers["TIME"] = &p_shader->canvas_item.uses_time;

            shaders.actions_canvas.usage_flag_pointers["MODULATE"] = &p_shader->canvas_item.uses_modulate;
            shaders.actions_canvas.usage_flag_pointers["COLOR"] = &p_shader->canvas_item.uses_color;
            shaders.actions_canvas.usage_flag_pointers["VERTEX"] = &p_shader->canvas_item.uses_vertex;

            shaders.actions_canvas.usage_flag_pointers["WORLD_MATRIX"] = &p_shader->canvas_item.uses_world_matrix;
            shaders.actions_canvas.usage_flag_pointers["EXTRA_MATRIX"] = &p_shader->canvas_item.uses_extra_matrix;
            shaders.actions_canvas.usage_flag_pointers["PROJECTION_MATRIX"] = &p_shader->canvas_item.uses_projection_matrix;
            shaders.actions_canvas.usage_flag_pointers["INSTANCE_CUSTOM"] = &p_shader->canvas_item.uses_instance_custom;

            actions = &shaders.actions_canvas;
            actions->uniforms = &p_shader->uniforms;

        } break;

        case RS::ShaderMode::SPATIAL: {

            p_shader->spatial.blend_mode = RasterizerShaderComponent::Node3D::BLEND_MODE_MIX;
            p_shader->spatial.depth_draw_mode = RasterizerShaderComponent::Node3D::DEPTH_DRAW_OPAQUE;
            p_shader->spatial.cull_mode = RasterizerShaderComponent::Node3D::CULL_MODE_BACK;
            p_shader->spatial.uses_alpha = false;
            p_shader->spatial.uses_alpha_scissor = false;
            p_shader->spatial.uses_discard = false;
            p_shader->spatial.unshaded = false;
            p_shader->spatial.no_depth_test = false;
            p_shader->spatial.uses_sss = false;
            p_shader->spatial.uses_time = false;
            p_shader->spatial.uses_vertex_lighting = false;
            p_shader->spatial.uses_screen_texture = false;
            p_shader->spatial.uses_depth_texture = false;
            p_shader->spatial.uses_vertex = false;
            p_shader->spatial.uses_tangent = false;
            p_shader->spatial.uses_ensure_correct_normals = false;
            p_shader->spatial.writes_modelview_or_projection = false;
            p_shader->spatial.uses_world_coordinates = false;

            shaders.actions_scene.render_mode_values["blend_add"] = Pair<int8_t *, int>(&p_shader->spatial.blend_mode, RasterizerShaderComponent::Node3D::BLEND_MODE_ADD);
            shaders.actions_scene.render_mode_values["blend_mix"] = Pair<int8_t *, int>(&p_shader->spatial.blend_mode, RasterizerShaderComponent::Node3D::BLEND_MODE_MIX);
            shaders.actions_scene.render_mode_values["blend_sub"] = Pair<int8_t *, int>(&p_shader->spatial.blend_mode, RasterizerShaderComponent::Node3D::BLEND_MODE_SUB);
            shaders.actions_scene.render_mode_values["blend_mul"] = Pair<int8_t *, int>(&p_shader->spatial.blend_mode, RasterizerShaderComponent::Node3D::BLEND_MODE_MUL);

            shaders.actions_scene.render_mode_values["depth_draw_opaque"] = Pair<int8_t *, int>(&p_shader->spatial.depth_draw_mode, RasterizerShaderComponent::Node3D::DEPTH_DRAW_OPAQUE);
            shaders.actions_scene.render_mode_values["depth_draw_always"] = Pair<int8_t *, int>(&p_shader->spatial.depth_draw_mode, RasterizerShaderComponent::Node3D::DEPTH_DRAW_ALWAYS);
            shaders.actions_scene.render_mode_values["depth_draw_never"] = Pair<int8_t *, int>(&p_shader->spatial.depth_draw_mode, RasterizerShaderComponent::Node3D::DEPTH_DRAW_NEVER);
            shaders.actions_scene.render_mode_values["depth_draw_alpha_prepass"] = Pair<int8_t *, int>(&p_shader->spatial.depth_draw_mode, RasterizerShaderComponent::Node3D::DEPTH_DRAW_ALPHA_PREPASS);

            shaders.actions_scene.render_mode_values["cull_front"] = Pair<int8_t *, int>(&p_shader->spatial.cull_mode, RasterizerShaderComponent::Node3D::CULL_MODE_FRONT);
            shaders.actions_scene.render_mode_values["cull_back"] = Pair<int8_t *, int>(&p_shader->spatial.cull_mode, RasterizerShaderComponent::Node3D::CULL_MODE_BACK);
            shaders.actions_scene.render_mode_values["cull_disabled"] = Pair<int8_t *, int>(&p_shader->spatial.cull_mode, RasterizerShaderComponent::Node3D::CULL_MODE_DISABLED);

            shaders.actions_scene.render_mode_values["async_visible"] = Pair<int8_t *, int>(&async_mode, (int)ShaderGLES3::ASYNC_MODE_VISIBLE);
            shaders.actions_scene.render_mode_values["async_hidden"] = Pair<int8_t *, int>(&async_mode, (int)ShaderGLES3::ASYNC_MODE_HIDDEN);
            shaders.actions_scene.render_mode_flags["unshaded"] = &p_shader->spatial.unshaded;
            shaders.actions_scene.render_mode_flags["depth_test_disable"] = &p_shader->spatial.no_depth_test;

            shaders.actions_scene.render_mode_flags["vertex_lighting"] = &p_shader->spatial.uses_vertex_lighting;
            shaders.actions_scene.render_mode_flags["world_vertex_coords"] = &p_shader->spatial.uses_world_coordinates;

            shaders.actions_scene.render_mode_flags["ensure_correct_normals"] = &p_shader->spatial.uses_ensure_correct_normals;


            shaders.actions_scene.usage_flag_pointers["ALPHA"] = &p_shader->spatial.uses_alpha;
            shaders.actions_scene.usage_flag_pointers["ALPHA_SCISSOR"] = &p_shader->spatial.uses_alpha_scissor;

            shaders.actions_scene.usage_flag_pointers["SSS_STRENGTH"] = &p_shader->spatial.uses_sss;
            shaders.actions_scene.usage_flag_pointers["DISCARD"] = &p_shader->spatial.uses_discard;
            shaders.actions_scene.usage_flag_pointers["SCREEN_TEXTURE"] = &p_shader->spatial.uses_screen_texture;
            shaders.actions_scene.usage_flag_pointers["DEPTH_TEXTURE"] = &p_shader->spatial.uses_depth_texture;
            shaders.actions_scene.usage_flag_pointers["TIME"] = &p_shader->spatial.uses_time;

                 // Use of any of these BUILTINS indicate the need for transformed tangents.
                 // This is needed to know when to transform tangents in software skinning.
            shaders.actions_scene.usage_flag_pointers["TANGENT"] = &p_shader->spatial.uses_tangent;
            shaders.actions_scene.usage_flag_pointers["NORMALMAP"] = &p_shader->spatial.uses_tangent;


            shaders.actions_scene.write_flag_pointers["MODELVIEW_MATRIX"] = &p_shader->spatial.writes_modelview_or_projection;
            shaders.actions_scene.write_flag_pointers["PROJECTION_MATRIX"] = &p_shader->spatial.writes_modelview_or_projection;
            shaders.actions_scene.write_flag_pointers["VERTEX"] = &p_shader->spatial.uses_vertex;

            actions = &shaders.actions_scene;
            actions->uniforms = &p_shader->uniforms;

        } break;
        case RS::ShaderMode::PARTICLES: {

            actions = &shaders.actions_particles;
            actions->uniforms = &p_shader->uniforms;
        } break;
        case RS::ShaderMode::MAX:
            break; // Can't happen, but silences warning
    }

    Error err = shaders.compiler.compile(p_shader->mode, p_shader->code, actions, p_shader->path, gen_code);

    if (err != OK) {
        return;
    }

    p_shader->ubo_size = gen_code.uniform_total_size;
    p_shader->ubo_offsets = eastl::move(gen_code.uniform_offsets);
    p_shader->texture_count = gen_code.texture_uniforms.size();
    p_shader->texture_hints = eastl::move(gen_code.texture_hints);
    p_shader->texture_types = eastl::move(gen_code.texture_types);

    p_shader->uses_vertex_time = gen_code.uses_vertex_time;
    p_shader->uses_fragment_time = gen_code.uses_fragment_time;

         // some logic for batching
    if (p_shader->mode == RS::ShaderMode::CANVAS_ITEM) {
        if (p_shader->canvas_item.uses_modulate | p_shader->canvas_item.uses_color) {
            p_shader->canvas_item.batch_flags |= RasterizerStorageCommon::PREVENT_COLOR_BAKING;
        }
        if (p_shader->canvas_item.uses_vertex) {
            p_shader->canvas_item.batch_flags |= RasterizerStorageCommon::PREVENT_VERTEX_BAKING;
        }
        if (p_shader->canvas_item.uses_world_matrix | p_shader->canvas_item.uses_extra_matrix | p_shader->canvas_item.uses_projection_matrix | p_shader->canvas_item.uses_instance_custom) {
            p_shader->canvas_item.batch_flags |= RasterizerStorageCommon::PREVENT_ITEM_JOINING;
        }
    }

    p_shader->shader->set_custom_shader_code(p_shader->custom_code_id, gen_code.vertex, gen_code.vertex_global,
            gen_code.fragment, gen_code.light, gen_code.fragment_global, gen_code.uniforms, gen_code.texture_uniforms,
            gen_code.defines,(ShaderGLES3::AsyncMode)async_mode);
         //all materials using this shader will have to be invalidated, unfortunately

    for (RenderingEntity E : p_shader->materials) {
        RasterizerMaterialComponent & m = VSG::ecs->registry.get<RasterizerMaterialComponent>(E);
        _material_make_dirty(&m);
    }

    p_shader->valid = true;
    p_shader->version++;
}

void RasterizerStorageGLES3::update_dirty_shaders() {
    auto vw = VSG::ecs->registry.view<ShaderDirtyMarker,RasterizerShaderComponent>();
    vw.each([&](auto entity, auto &shader) {
        _update_shader(shaders,&shader);
    });
}

void RasterizerStorageGLES3::shader_get_param_list(RenderingEntity p_shader, Vector<PropertyInfo> *p_param_list) const {

    RasterizerShaderComponent *shader = getUnchecked<RasterizerShaderComponent>(p_shader);
    ERR_FAIL_COND(!shader);

    if (VSG::ecs->registry.any_of<ShaderDirtyMarker>(p_shader))
        _update_shader(shaders,shader); // ok should be not anymore dirty

    Map<int, StringName> order;

    for (eastl::pair<const StringName,ShaderLanguage::ShaderNode::Uniform> &E : shader->uniforms) {

        if (E.second.texture_order >= 0) {
            order[E.second.texture_order + 100000] = E.first;
        } else {
            order[E.second.order] = E.first;
        }
    }
    p_param_list->reserve(p_param_list->size()+order.size());
    for (const eastl::pair<const int,StringName> &E : order) {

        PropertyInfo pi;
        ShaderLanguage::ShaderNode::Uniform &u = shader->uniforms[E.second];
        pi.name = E.second;
        switch (u.type) {
        case ShaderLanguage::TYPE_STRUCT:
            pi.type = VariantType::ARRAY;
            break;
        case ShaderLanguage::TYPE_VOID:
            pi.type = VariantType::NIL;
            break;
        case ShaderLanguage::TYPE_BOOL:
            pi.type = VariantType::BOOL;
            break;
            case ShaderLanguage::TYPE_BVEC2:
                pi.type = VariantType::INT;
                pi.hint = PropertyHint::Flags;
                pi.hint_string = "x,y";
                break;
            case ShaderLanguage::TYPE_BVEC3:
                pi.type = VariantType::INT;
                pi.hint = PropertyHint::Flags;
                pi.hint_string = "x,y,z";
                break;
            case ShaderLanguage::TYPE_BVEC4:
                pi.type = VariantType::INT;
                pi.hint = PropertyHint::Flags;
                pi.hint_string = "x,y,z,w";
                break;
            case ShaderLanguage::TYPE_UINT:
            case ShaderLanguage::TYPE_INT: {
                pi.type = VariantType::INT;
                if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_RANGE) {
                    pi.hint = PropertyHint::Range;
                    char buf[128];
                    snprintf(buf,127,"%f,%f,%f", u.hint_range[0], u.hint_range[1], u.hint_range[2]);
                    pi.hint_string = buf;
                }

            } break;
            case ShaderLanguage::TYPE_IVEC2:
            case ShaderLanguage::TYPE_IVEC3:
            case ShaderLanguage::TYPE_IVEC4:
            case ShaderLanguage::TYPE_UVEC2:
            case ShaderLanguage::TYPE_UVEC3:
            case ShaderLanguage::TYPE_UVEC4: {

                pi.type = VariantType::POOL_INT_ARRAY;
            } break;
            case ShaderLanguage::TYPE_FLOAT: {
                pi.type = VariantType::FLOAT;
                if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_RANGE) {
                    pi.hint = PropertyHint::Range;
                    char buf[128];
                    snprintf(buf, 127, "%f,%f,%f", u.hint_range[0], u.hint_range[1], u.hint_range[2]);
                    pi.hint_string = buf;
                }

            } break;
            case ShaderLanguage::TYPE_VEC2: pi.type = VariantType::VECTOR2; break;
            case ShaderLanguage::TYPE_VEC3: pi.type = VariantType::VECTOR3; break;
            case ShaderLanguage::TYPE_VEC4: {
                if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_COLOR) {
                    pi.type = VariantType::COLOR;
                } else {
                    pi.type = VariantType::PLANE;
                }
            } break;
            case ShaderLanguage::TYPE_MAT2: pi.type = VariantType::TRANSFORM2D; break;
            case ShaderLanguage::TYPE_MAT3: pi.type = VariantType::BASIS; break;
            case ShaderLanguage::TYPE_MAT4: pi.type = VariantType::TRANSFORM; break;
            case ShaderLanguage::TYPE_SAMPLER2D:
            case ShaderLanguage::TYPE_SAMPLEREXT:
            case ShaderLanguage::TYPE_ISAMPLER2D:
            case ShaderLanguage::TYPE_USAMPLER2D: {

                pi.type = VariantType::OBJECT;
                pi.hint = PropertyHint::ResourceType;
                pi.hint_string = "Texture";
            } break;
            case ShaderLanguage::TYPE_SAMPLER2DARRAY:
            case ShaderLanguage::TYPE_ISAMPLER2DARRAY:
            case ShaderLanguage::TYPE_USAMPLER2DARRAY: {

                pi.type = VariantType::OBJECT;
                pi.hint = PropertyHint::ResourceType;
                pi.hint_string = "TextureArray";
            } break;
            case ShaderLanguage::TYPE_SAMPLER3D:
            case ShaderLanguage::TYPE_ISAMPLER3D:
            case ShaderLanguage::TYPE_USAMPLER3D: {
                pi.type = VariantType::OBJECT;
                pi.hint = PropertyHint::ResourceType;
                pi.hint_string = "Texture3D";
            } break;
            case ShaderLanguage::TYPE_SAMPLERCUBE: {

                pi.type = VariantType::OBJECT;
                pi.hint = PropertyHint::ResourceType;
                pi.hint_string = "CubeMap";
            } break;
        }

        p_param_list->push_back(pi);
    }
}

void RasterizerStorageGLES3::shader_set_default_texture_param(RenderingEntity p_shader, const StringName &p_name, RenderingEntity p_texture) {

    RasterizerShaderComponent *shader = getUnchecked<RasterizerShaderComponent>(p_shader);
    ERR_FAIL_COND(!shader);
    ERR_FAIL_COND(p_texture!=entt::null && !VSG::ecs->registry.any_of<RasterizerTextureComponent>(p_texture));

    if (p_texture!=entt::null) {
        shader->default_textures[p_name] = p_texture;
    } else {
        shader->default_textures.erase(p_name);
    }

    _shader_make_dirty(shader);
}
RenderingEntity RasterizerStorageGLES3::shader_get_default_texture_param(RenderingEntity p_shader, const StringName &p_name) const {

    const RasterizerShaderComponent *shader = getUnchecked<RasterizerShaderComponent>(p_shader);
    ERR_FAIL_COND_V(!shader, entt::null);

    auto E = shader->default_textures.find(p_name);
    if (E==shader->default_textures.end())
        return entt::null;
    return E->second;
}

void RasterizerStorageGLES3::shader_add_custom_define(RenderingEntity p_shader, StringView p_define) {

    RasterizerShaderComponent *shader = getUnchecked<RasterizerShaderComponent>(p_shader);
    ERR_FAIL_COND(!shader);

    shader->shader->add_custom_define(p_define);

    _shader_make_dirty(shader);
}

void RasterizerStorageGLES3::shader_get_custom_defines(RenderingEntity p_shader, Vector<StringView> *p_defines) const {

    RasterizerShaderComponent *shader = getUnchecked<RasterizerShaderComponent>(p_shader);
    ERR_FAIL_COND(!shader);

    shader->shader->get_custom_defines(p_defines);
}

void RasterizerStorageGLES3::shader_remove_custom_define(RenderingEntity p_shader, StringView p_define) {

    RasterizerShaderComponent *shader = getUnchecked<RasterizerShaderComponent>(p_shader);
    ERR_FAIL_COND(!shader);

    shader->shader->remove_custom_define(p_define);

    _shader_make_dirty(shader);
}

void RasterizerStorageGLES3::set_shader_async_hidden_forbidden(bool p_forbidden) {
    ShaderGLES3::async_hidden_forbidden = p_forbidden;
}

bool RasterizerStorageGLES3::is_shader_async_hidden_forbidden() {
    return ShaderGLES3::async_hidden_forbidden;
}

RasterizerShaderComponent &RasterizerShaderComponent::operator=(RasterizerShaderComponent && from)
{

    // free custom shader and mark materials using this as dirty.
    if (shader && custom_code_id) {
        shader->free_custom_shader(this->custom_code_id);
    }

    for(RenderingEntity mat : materials) {
        auto *p_mat = VSG::ecs->try_get<RasterizerMaterialComponent>(mat);
        p_mat->shader = entt::null;
        _material_make_dirty(p_mat);
    }
    if(this==&from) { // we clear 'this' to correctly handle self assignment
        uniforms.clear();
        default_textures.clear();
        ubo_offsets.clear();
        texture_types.clear();
        texture_hints.clear();
        materials.clear();
        code.clear();
        path.clear();
    }

    version = eastl::move(from.version);
    uniforms = eastl::move(from.uniforms);
    default_textures = eastl::move(from.default_textures);
    ubo_offsets = eastl::move(from.ubo_offsets);
    texture_types = eastl::move(from.texture_types);
    texture_hints = eastl::move(from.texture_hints);
    materials = eastl::move(from.materials); //RasterizerMaterialComponent
    code = eastl::move(from.code);
    path = eastl::move(from.path);
    self = eastl::move(from.self);
    shader = eastl::move(from.shader);
    mode = eastl::move(from.mode);
    ubo_size = eastl::move(from.ubo_size);
    texture_count = eastl::move(from.texture_count);
    custom_code_id = eastl::move(from.custom_code_id);
    valid = eastl::move(from.valid);
    canvas_item = eastl::move(from.canvas_item);
    spatial = eastl::move(from.spatial);
    particles = eastl::move(from.particles);
    uses_vertex_time = eastl::move(from.uses_vertex_time);
    uses_fragment_time = eastl::move(from.uses_fragment_time);
    from.shader = nullptr;
    return *this;
}

RasterizerShaderComponent::~RasterizerShaderComponent()
{
    // free custom shader and mark materials using this as dirty.

    if (shader && custom_code_id) {
        shader->free_custom_shader(custom_code_id);
    }

    for(RenderingEntity referencing_mat : materials) {
        auto *mat = &VSG::ecs->registry.get<RasterizerMaterialComponent>(referencing_mat);
        mat->shader = entt::null;
        _material_make_dirty(mat);

    }
    materials.clear();

}
