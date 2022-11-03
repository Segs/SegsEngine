/*************************************************************************/
/*  color.h                                                              */
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

#include "core/godot_export.h"
#include "core/forward_decls.h"
#include "core/math/math_funcs.h"

struct GODOT_EXPORT [[nodiscard]] Color {

    float r=0.0f;
    float g=0.0f;
    float b=0.0f;
    float a=1.0f;

    constexpr bool operator==(Color p_color) const { return (r == p_color.r && g == p_color.g && b == p_color.b && a == p_color.a); }
    constexpr bool operator!=(Color p_color) const { return (r != p_color.r || g != p_color.g || b != p_color.b || a != p_color.a); }

    uint32_t to_rgba32() const;
    uint32_t to_argb32() const;
    uint32_t to_abgr32() const;
    uint64_t to_rgba64() const;
    uint64_t to_argb64() const;
    uint64_t to_abgr64() const;

    float get_h() const;
    float get_s() const;
    float get_v() const;
    void set_hsv(float p_h, float p_s, float p_v, float p_alpha = 1.0);

    constexpr float *components() { return &r; }

    constexpr Color operator+(Color p_color) const {
        return Color(r + p_color.r, g + p_color.g, b + p_color.b, a + p_color.a);
    }
    constexpr void operator+=(Color p_color) {
        r += p_color.r;
        g += p_color.g;
        b += p_color.b;
        a += p_color.a;
    }

    constexpr Color operator-() const {
        return Color(1.0f - r, 1.0f - g, 1.0f - b, 1.0f - a);
    }

    constexpr Color operator-(Color p_color) const {
        return Color(r - p_color.r, g - p_color.g, b - p_color.b, a - p_color.a);
    }

    constexpr Color operator-=(Color p_color) {
        r = r - p_color.r;
        g = g - p_color.g;
        b = b - p_color.b;
        a = a - p_color.a;
        return *this;
    }

    constexpr Color operator*(Color p_color) const {
        return Color(r * p_color.r, g * p_color.g, b * p_color.b, a * p_color.a);
    }
    constexpr Color operator*(float rvalue) const {
        return Color(r * rvalue, g * rvalue, b * rvalue, a * rvalue);
    }

    void operator*=(Color p_color);
    void operator*=(float rvalue);

    Color operator/(Color p_color) const;
    Color operator/(float rvalue) const;
    void operator/=(Color p_color);
    void operator/=(float rvalue);

    bool is_equal_approx(Color p_color) const;
    void constexpr invert() {

        r = 1.0f - r;
        g = 1.0f - g;
        b = 1.0f - b;
    }
    constexpr Color inverted() const {
        return Color(1.0f-r,1.0f-g,1.0f-b);
    }
    void contrast();
    Color contrasted() const;

    constexpr float get_luminance() const {
        return 0.2126 * r + 0.7152 * g + 0.0722 * b;
    }

    constexpr Color linear_interpolate(Color p_to, float p_weight) const {

        Color res = *this;

        res.r += (p_weight * (p_to.r - r));
        res.g += (p_weight * (p_to.g - g));
        res.b += (p_weight * (p_to.b - b));
        res.a += (p_weight * (p_to.a - a));

        return res;
    }

    constexpr Color darkened(float p_amount) const {
        return Color(
            r * (1.0f - p_amount),
            g * (1.0f - p_amount),
            b * (1.0f - p_amount)
        );
    }

    constexpr Color lightened(float p_amount) const {
        return Color(
            r + (1.0f - r) * p_amount,
            g + (1.0f - g) * p_amount,
            b + (1.0f - b) * p_amount
        );
    }

    uint32_t to_rgbe9995() const;

    constexpr Color blend(Color p_over) const {

        Color res;
        float sa = 1.0f - p_over.a;
        res.a = a * sa + p_over.a;
        if (res.a == 0.0f) {
            return Color(0, 0, 0, 0);
        } else {
            res.r = (r * a * sa + p_over.r * p_over.a) / res.a;
            res.g = (g * a * sa + p_over.g * p_over.a) / res.a;
            res.b = (b * a * sa + p_over.b * p_over.a) / res.a;
        }
        return res;
    }

    constexpr Color to_linear() const {

        return Color(
                r < 0.04045f ? r * (1.0f / 12.92f) : Math::pow((r + 0.055f) * (1.0f / (1 + 0.055f)), 2.4f),
                g < 0.04045f ? g * (1.0f / 12.92f) : Math::pow((g + 0.055f) * (1.0f / (1 + 0.055f)), 2.4f),
                b < 0.04045f ? b * (1.0f / 12.92f) : Math::pow((b + 0.055f) * (1.0f / (1 + 0.055f)), 2.4f),
                a);
    }
    constexpr Color to_srgb() const {

        return Color(
                r < 0.0031308f ? 12.92f * r : (1.0f + 0.055f) * Math::pow(r, 1.0f / 2.4f) - 0.055f,
                g < 0.0031308f ? 12.92f * g : (1.0f + 0.055f) * Math::pow(g, 1.0f / 2.4f) - 0.055f,
                b < 0.0031308f ? 12.92f * b : (1.0f + 0.055f) * Math::pow(b, 1.0f / 2.4f) - 0.055f, a);
    }

    static Color hex(uint32_t p_hex);
    static Color hex64(uint64_t p_hex);
    static Color html(StringView p_color);
    static bool html_is_valid(StringView p_color);
    static Color named(StringView p_name);
    String to_html(bool p_alpha = true) const;
    static Color from_hsv(float p_h, float p_s, float p_v, float p_a);
    static Color from_rgbe9995(uint32_t p_rgbe);

    //used in set keys
    bool operator<(Color p_color) const {

        if (r == p_color.r) {
            if (g == p_color.g) {
                if (b == p_color.b) {
                    return (a < p_color.a);
                } else
                    return (b < p_color.b);
            } else
                return g < p_color.g;
        } else
            return r < p_color.r;
    }
    operator String() const;
    constexpr float &component(uint8_t idx) { return (&r)[idx]; }
    constexpr float component(uint8_t idx) const { return (&r)[idx]; }
    /**
     * No construct parameters, r=0, g=0, b=0. a=255
     */
    constexpr Color() = default;

    /**
     * RGB / RGBA construct parameters. Alpha is optional, but defaults to 1.0
     */
    constexpr Color(float p_r, float p_g, float p_b, float p_a = 1.0) : r(p_r),g(p_g),b(p_b),a(p_a) {
    }
    /**
     * Construct a Color from another Color, but with the specified alpha value.
     */
    constexpr Color(Color p_c, float p_a) : r(p_c.r),g(p_c.g),b(p_c.b),a(p_a) {
    }
};


