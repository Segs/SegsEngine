/*************************************************************************/
/*  shader.h                                                             */
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

#include "core/hash_map.h"
#include "core/resource.h"
#include "core/string.h"
#include "scene/resources/texture.h"

namespace RenderingServerEnums {
enum class ShaderMode : int8_t;
}

class GODOT_EXPORT Shader : public Resource {

    GDCLASS(Shader,Resource)

    OBJ_SAVE_TYPE(Shader)

private:
    RenderingEntity shader;
    RenderingServerEnums::ShaderMode mode;
    String shader_custom_defines;

    // hack the name of performance
    // shaders keep a list of ShaderMaterial -> RenderingServer name translations, to make
    // conversion fast and save memory.
    mutable bool params_cache_dirty;
    mutable HashMap<StringName, StringName> params_cache; //map a shader param to a material param..
    //TODO: SEGS: was a name->Ref<Texture> map, but default texture can also be CubeMap
    HashMap<StringName, Ref<Resource> > default_textures;

    virtual void _update_shader() const; //used for visual shader
protected:
    static void _bind_methods();

public:
    //void set_mode(Mode p_mode);
    virtual RenderingServerEnums::ShaderMode get_mode() const;

    void set_code(const String &p_code);
    String get_code() const;

    void get_param_list(Vector<PropertyInfo> *p_params) const;
    bool has_param(const StringName &p_param) const;
    //TODO: SEGS: was `Ref<Texture> &p_texture` but can also use CubeMap so it's using common base of Texture/CubeMap, consider introducing a common ?ImageSource? base class?
    void set_default_texture_param(const StringName &p_param, const Ref<Resource> &p_texture);
    Ref<Resource> get_default_texture_param(const StringName &p_param) const;
    void get_default_texture_param_list(List<StringName> *r_textures) const;

    void set_custom_defines(StringView p_defines);
    String get_custom_defines() const;

    virtual bool is_text_shader() const;

    _FORCE_INLINE_ StringName remap_param(const StringName &p_param) const {
        if (params_cache_dirty)
            get_param_list(nullptr);

        auto E = params_cache.find(p_param);
        if (E!=params_cache.end())
            return E->second;
        return StringName();
    }

    RenderingEntity get_rid() const override;

    Shader();
    ~Shader() override;
};

