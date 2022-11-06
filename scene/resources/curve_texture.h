/*************************************************************************/
/*  curve_texture.h                                                      */
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
#include "scene/resources/curve.h"
#include "scene/resources/texture.h"

class GODOT_EXPORT CurveTexture : public Texture {

    GDCLASS(CurveTexture,Texture)
    RES_BASE_EXTENSION("curvetex")

private:
    RenderingEntity _texture;
    Ref<Curve> _curve;
    int _width;

    void _update();

protected:
    static void _bind_methods();

public:
    void set_width(int p_width);
    int get_width() const override;

    void ensure_default_setup(float p_min = 0, float p_max = 1);

    void set_curve(const Ref<Curve>& p_curve);
    Ref<Curve> get_curve() const;

    RenderingEntity get_rid() const override;

    int get_height() const override { return 1; }
    bool has_alpha() const override { return false; }

    void set_flags(uint32_t /*p_flags*/) override {}
    uint32_t get_flags() const override { return FLAG_FILTER; }

    CurveTexture();
    ~CurveTexture() override;
};
