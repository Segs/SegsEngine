/*************************************************************************/
/*  canvas_item.h                                                        */
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

#include "scene/resources/material.h"

class GODOT_EXPORT CanvasItemMaterial : public Material {

    GDCLASS(CanvasItemMaterial,Material)

public:
    enum BlendMode : uint8_t {
        BLEND_MODE_MIX,
        BLEND_MODE_ADD,
        BLEND_MODE_SUB,
        BLEND_MODE_MUL,
        BLEND_MODE_PREMULT_ALPHA,
        BLEND_MODE_DISABLED
    };

    enum LightMode : uint8_t {
        LIGHT_MODE_NORMAL,
        LIGHT_MODE_UNSHADED,
        LIGHT_MODE_LIGHT_ONLY
    };

private:
    union MaterialKey {

        struct {
            uint32_t blend_mode : 4;
            uint32_t light_mode : 4;
            uint32_t particles_animation : 1;
            uint32_t invalid_key : 1;
        };

        uint32_t key;

        bool operator==(MaterialKey p_key) const {
            return key == p_key.key;
        }
    private:
        friend eastl::hash<MaterialKey>;
        explicit operator size_t() const {
            return key;
        }

    };

    struct ShaderData {
        RenderingEntity shader;
        int users;
    };



    static HashMap<MaterialKey, ShaderData> shader_map;
    static Mutex material_mutex;

    MaterialKey current_key;
    bool is_dirty_element;
    bool is_initialized = false;

    int particles_anim_h_frames;
    int particles_anim_v_frames;

    BlendMode blend_mode;
    LightMode light_mode;
    bool particles_animation;
    bool particles_anim_loop;

    _FORCE_INLINE_ MaterialKey _compute_key() const {

        MaterialKey mk;
        mk.key = 0;
        mk.blend_mode = blend_mode;
        mk.light_mode = light_mode;
        mk.particles_animation = particles_animation;
        return mk;
    }
    void _update_shader();
    _FORCE_INLINE_ void _queue_shader_change();
protected:
    static void _bind_methods();
    void _validate_property(PropertyInfo &property) const override;

public:
    void set_blend_mode(BlendMode p_blend_mode);
    BlendMode get_blend_mode() const;

    void set_light_mode(LightMode p_light_mode);
    LightMode get_light_mode() const;

    void set_particles_animation(bool p_particles_anim);
    bool get_particles_animation() const;

    void set_particles_anim_h_frames(int p_frames);
    int get_particles_anim_h_frames() const;
    void set_particles_anim_v_frames(int p_frames);
    int get_particles_anim_v_frames() const;

    void set_particles_anim_loop(bool p_loop);
    bool get_particles_anim_loop() const;

    static void init_shaders();
    static void finish_shaders();
    static void flush_changes();

    RenderingEntity get_shader_rid() const;

    RenderingServerEnums::ShaderMode get_shader_mode() const override;

    CanvasItemMaterial();
    ~CanvasItemMaterial() override;
};
