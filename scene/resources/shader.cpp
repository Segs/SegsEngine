/*************************************************************************/
/*  shader.cpp                                                           */
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

#include "shader.h"
#include "shader_serialization.h"

#include "texture.h"
#include "shader_enum_casters.h"

#include "core/method_bind.h"
#include "core/os/file_access.h"
#include "scene/scene_string_names.h"
#include "servers/visual/shader_language.h"
#include "servers/visual_server.h"

IMPL_GDCLASS(Shader)

ShaderMode Shader::get_mode() const {

    return mode;
}

void Shader::set_code(const String &p_code) {

    String type = ShaderLanguage::get_shader_type(p_code);

    if (type == "canvas_item") {
        mode = ShaderMode::CANVAS_ITEM;
    } else if (type == "particles") {
        mode = ShaderMode::PARTICLES;
    } else {
        mode = ShaderMode::SPATIAL;
    }

    VisualServer::get_singleton()->shader_set_code(shader, p_code);
    params_cache_dirty = true;

    emit_changed();
}

String Shader::get_code() const {

    _update_shader();
    return VisualServer::get_singleton()->shader_get_code(shader);
}

void Shader::get_param_list(ListPOD<PropertyInfo> *p_params) const {

    _update_shader();

    PODVector<PropertyInfo> local;
    VisualServer::get_singleton()->shader_get_param_list(shader, &local);
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
            if (pi.type == VariantType::_RID)
                pi.type = VariantType::OBJECT;
            p_params->push_back(pi);
        }
    }
}

RID Shader::get_rid() const {

    _update_shader();

    return shader;
}

void Shader::set_default_texture_param(const StringName &p_param, const Ref<Resource> &p_texture) {

    if (p_texture) {
        default_textures[p_param] = p_texture;
        VisualServer::get_singleton()->shader_set_default_texture_param(shader, p_param, p_texture->get_rid());
    } else {
        default_textures.erase(p_param);
        VisualServer::get_singleton()->shader_set_default_texture_param(shader, p_param, RID());
    }

    emit_changed();
}

Ref<Resource> Shader::get_default_texture_param(const StringName &p_param) const {

    return default_textures.at(p_param,Ref<Resource>());
}

void Shader::get_default_texture_param_list(ListPOD<StringName> *r_textures) const {

    for (const eastl::pair<const StringName,Ref<Resource> > &E : default_textures) {

        r_textures->push_back(E.first);
    }
}

bool Shader::is_text_shader() const {
    return true;
}

bool Shader::has_param(const StringName &p_param) const {

    return params_cache.contains(p_param);
}

void Shader::_update_shader() const {
}

void Shader::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("get_mode"), &Shader::get_mode);

    MethodBinder::bind_method(D_METHOD("set_code", {"code"}), &Shader::set_code);
    MethodBinder::bind_method(D_METHOD("get_code"), &Shader::get_code);

    MethodBinder::bind_method(D_METHOD("set_default_texture_param", {"param", "texture"}), &Shader::set_default_texture_param);
    MethodBinder::bind_method(D_METHOD("get_default_texture_param", {"param"}), &Shader::get_default_texture_param);

    MethodBinder::bind_method(D_METHOD("has_param", {"name"}), &Shader::has_param);

    //MethodBinder::bind_method(D_METHOD("get_param_list"),&Shader::get_fragment_code);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "code", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), "set_code", "get_code");
#ifdef DEBUG_METHODS_ENABLED
    ClassDB::bind_integer_constant(get_class_static_name(), __constant_get_enum_name(ShaderMode::SPATIAL, "MODE_SPATIAL"), "MODE_SPATIAL", (int)ShaderMode::SPATIAL);
    ClassDB::bind_integer_constant(get_class_static_name(), __constant_get_enum_name(ShaderMode::CANVAS_ITEM, "MODE_CANVAS_ITEM"), "MODE_CANVAS_ITEM", (int)ShaderMode::CANVAS_ITEM);
    ClassDB::bind_integer_constant(get_class_static_name(), __constant_get_enum_name(ShaderMode::PARTICLES, "MODE_PARTICLES"), "MODE_PARTICLES", (int)ShaderMode::PARTICLES);
#endif
}

Shader::Shader() {

    mode = ShaderMode::SPATIAL;
    shader = VisualServer::get_singleton()->shader_create();
    params_cache_dirty = true;
}

Shader::~Shader() {

    VisualServer::get_singleton()->free(shader);
}
////////////

RES ResourceFormatLoaderShader::load(const String &p_path, const String &p_original_path, Error *r_error) {

    if (r_error)
        *r_error = ERR_FILE_CANT_OPEN;

    Ref<Shader> shader(make_ref_counted<Shader>());

    Vector<uint8_t> buffer = FileAccess::get_file_as_array(p_path);

    String str = StringUtils::from_utf8((const char *)buffer.ptr(), buffer.size());

    shader->set_code(str);

    if (r_error)
        *r_error = OK;

    return shader;
}

void ResourceFormatLoaderShader::get_recognized_extensions(ListPOD<String> *p_extensions) const {

    p_extensions->push_back("shader");
}

bool ResourceFormatLoaderShader::handles_type(const String &p_type) const {

    return (p_type == "Shader");
}

String ResourceFormatLoaderShader::get_resource_type(const String &p_path) const {

    String el = StringUtils::to_lower(PathUtils::get_extension(p_path));
    if (el == "shader")
        return "Shader";
    return "";
}

Error ResourceFormatSaverShader::save(const String &p_path, const RES &p_resource, uint32_t p_flags) {

    Ref<Shader> shader = dynamic_ref_cast<Shader>(p_resource);
    ERR_FAIL_COND_V(not shader, ERR_INVALID_PARAMETER)

    String source = shader->get_code();

    Error err;
    FileAccess *file = FileAccess::open(p_path, FileAccess::WRITE, &err);

    if (err) {

        ERR_FAIL_COND_V(err, err)
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

void ResourceFormatSaverShader::get_recognized_extensions(const RES &p_resource, Vector<String> *p_extensions) const {

    if (const Shader *shader = object_cast<Shader>(p_resource.get())) {
        if (shader->is_text_shader()) {
            p_extensions->push_back("shader");
        }
    }
}
bool ResourceFormatSaverShader::recognize(const RES &p_resource) const {

    return p_resource->get_class_name() == "Shader"; //only shader, not inherited
}
