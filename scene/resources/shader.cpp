/*************************************************************************/
/*  shader.cpp                                                           */
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

#include "shader.h"
#include "shader_serialization.h"

#include "texture.h"

#include "core/method_bind.h"
#include "core/os/file_access.h"
#include "scene/scene_string_names.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering_server.h"
#include "servers/rendering_server_enum_casters.h"

IMPL_GDCLASS(Shader)

RenderingServerEnums::ShaderMode Shader::get_mode() const {

    return mode;
}

void Shader::set_code(const String &p_code) {

    String type(ShaderLanguage::get_shader_type(p_code));

    if (type == "canvas_item") {
        mode = RenderingServerEnums::ShaderMode::CANVAS_ITEM;
    } else if (type == "particles") {
        mode = RenderingServerEnums::ShaderMode::PARTICLES;
    } else {
        mode = RenderingServerEnums::ShaderMode::SPATIAL;
    }

    RenderingServer::get_singleton()->shader_set_code(shader, p_code);
    params_cache_dirty = true;

    emit_changed();
}

String Shader::get_code() const {

    _update_shader();
    return RenderingServer::get_singleton()->shader_get_code(shader);
}

void Shader::get_param_list(Vector<PropertyInfo> *p_params) const {

    _update_shader();

    Vector<PropertyInfo> local;
    RenderingServer::get_singleton()->shader_get_param_list(shader, &local);
    params_cache.clear();
    params_cache_dirty = false;

    for (const PropertyInfo &E : local) {

        PropertyInfo pi = E;
        if (default_textures.contains(pi.name)) { //do not show default textures
            continue;
        }
        pi.name = "shader_param/" + pi.name;
        params_cache[pi.name] = E.name;
        if (p_params) {

            //small little hack
            if (pi.type == VariantType::_RID) {
                pi.type = VariantType::OBJECT;
            }
            p_params->push_back(pi);
        }
    }
}

RenderingEntity Shader::get_rid() const {

    _update_shader();

    return shader;
}

void Shader::set_default_texture_param(const StringName &p_param, const Ref<Resource> &p_texture) {

    if (p_texture) {
        default_textures[p_param] = p_texture;
        RenderingServer::get_singleton()->shader_set_default_texture_param(shader, p_param, p_texture->get_rid());
    } else {
        default_textures.erase(p_param);
        RenderingServer::get_singleton()->shader_set_default_texture_param(shader, p_param, entt::null);
    }

    emit_changed();
}

Ref<Resource> Shader::get_default_texture_param(const StringName &p_param) const {

    return default_textures.at(p_param,Ref<Resource>());
}

void Shader::get_default_texture_param_list(List<StringName> *r_textures) const {

    for (const eastl::pair<const StringName,Ref<Resource> > &E : default_textures) {

        r_textures->push_back(E.first);
    }
}
void Shader::set_custom_defines(StringView p_defines) {
    if (shader_custom_defines == p_defines) {
        return;
    }

    if (!shader_custom_defines.empty()) {
        RenderingServer::get_singleton()->shader_remove_custom_define(shader, shader_custom_defines);
    }

    shader_custom_defines = p_defines;
    RenderingServer::get_singleton()->shader_add_custom_define(shader, shader_custom_defines);
}

String Shader::get_custom_defines() const {
    return shader_custom_defines;
}

bool Shader::is_text_shader() const {
    return true;
}

bool Shader::has_param(const StringName &p_param) const {

    return params_cache.contains_as(String("shader_param/")+p_param);
}

void Shader::_update_shader() const {
}

void Shader::_bind_methods() {

    SE_BIND_METHOD(Shader,get_mode);

    SE_BIND_METHOD(Shader,set_code);
    SE_BIND_METHOD(Shader,get_code);

    SE_BIND_METHOD(Shader,set_default_texture_param);
    SE_BIND_METHOD(Shader,get_default_texture_param);

    SE_BIND_METHOD(Shader,set_custom_defines);
    SE_BIND_METHOD(Shader,get_custom_defines);

    SE_BIND_METHOD(Shader,has_param);

    //MethodBinder::bind_method(D_METHOD("get_param_list"),&Shader::get_fragment_code);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "code", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_code", "get_code");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "custom_defines", PropertyHint::MultilineText), "set_custom_defines", "get_custom_defines");

}

Shader::Shader() {

    mode = RenderingServerEnums::ShaderMode::SPATIAL;
    shader = RenderingServer::get_singleton()->shader_create();
    params_cache_dirty = true;
}

Shader::~Shader() {

    RenderingServer::get_singleton()->free_rid(shader);
}
////////////

RES ResourceFormatLoaderShader::load(StringView p_path, StringView p_original_path, Error *r_error, bool p_no_subresource_cache) {

    if (r_error) {
        *r_error = ERR_FILE_CANT_OPEN;
    }

    Ref<Shader> shader(make_ref_counted<Shader>());

    String str = FileAccess::get_file_as_string(p_path);

    shader->set_code(str);

    if (r_error) {
        *r_error = OK;
    }

    return shader;
}

void ResourceFormatLoaderShader::get_recognized_extensions(Vector<String> &p_extensions) const {
    p_extensions.push_back("gdshader");
    p_extensions.push_back("shader");
}

bool ResourceFormatLoaderShader::handles_type(StringView p_type) const {

    return p_type == StringView("Shader");
}

String ResourceFormatLoaderShader::get_resource_type(StringView p_path) const {

    String el = StringUtils::to_lower(PathUtils::get_extension(p_path));
    if (el == "gdshader" || el == "shader") {
        return "Shader";
    }
    return String();
}

Error ResourceFormatSaverShader::save(StringView p_path, const RES &p_resource, uint32_t p_flags) {

    Ref<Shader> shader = dynamic_ref_cast<Shader>(p_resource);
    ERR_FAIL_COND_V(not shader, ERR_INVALID_PARAMETER);

    String source(shader->get_code());

    Error err;
    FileAccess *file = FileAccess::open(p_path, FileAccess::WRITE, &err);

    if (err) {

        ERR_FAIL_COND_V(err, err);
    }

    file->store_string(source);
    if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
        memdelete(file);
        return ERR_CANT_CREATE;
    }
    file->close();
    memdelete(file);

    return OK;
}

void ResourceFormatSaverShader::get_recognized_extensions(const RES &p_resource, Vector<String> &p_extensions) const {

    if (const Shader *shader = object_cast<Shader>(p_resource.get())) {
        if (shader->is_text_shader()) {
            p_extensions.emplace_back("gdshader");
            p_extensions.emplace_back("shader");
        }
    }
}
bool ResourceFormatSaverShader::recognize(const RES &p_resource) const {

    return p_resource->get_class_name() == "Shader"; //only shader, not inherited
}
