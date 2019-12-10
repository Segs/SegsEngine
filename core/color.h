/*************************************************************************/
/*  color.h                                                              */
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

#include "core/math/math_funcs.h"
#include "core/forward_decls.h"

struct GODOT_EXPORT Color {

    union {

        struct {
            float r;
            float g;
            float b;
            float a;
        };
        float components[4];
    };

    constexpr bool operator==(Color p_color) const { return (r == p_color.r && g == p_color.g && b == p_color.b && a == p_color.a); }
    constexpr bool operator!=(Color p_color) const { return (r != p_color.r || g != p_color.g || b != p_color.b || a != p_color.a); }

    [[nodiscard]] uint32_t to_rgba32() const;
    [[nodiscard]] uint32_t to_argb32() const;
    [[nodiscard]] uint32_t to_abgr32() const;
    [[nodiscard]] uint64_t to_rgba64() const;
    [[nodiscard]] uint64_t to_argb64() const;
    [[nodiscard]] uint64_t to_abgr64() const;

    [[nodiscard]] float get_h() const;
    [[nodiscard]] float get_s() const;
    [[nodiscard]] float get_v() const;
    void set_hsv(float p_h, float p_s, float p_v, float p_alpha = 1.0);

    _FORCE_INLINE_ float &operator[](int idx) {
        return components[idx];
    }
    _FORCE_INLINE_ float operator[](int idx) const {
        return components[idx];
    }

    Color operator+(const Color &p_color) const;
    void operator+=(const Color &p_color);

    Color operator-() const;
    Color operator-(const Color &p_color) const;
    void operator-=(const Color &p_color);

    Color operator*(const Color &p_color) const;
    Color operator*(const real_t &rvalue) const;
    void operator*=(const Color &p_color);
    void operator*=(const real_t &rvalue);

    Color operator/(const Color &p_color) const;
    Color operator/(const real_t &rvalue) const;
    void operator/=(const Color &p_color);
    void operator/=(const real_t &rvalue);

    void invert();
    void contrast();
    [[nodiscard]] Color inverted() const;
    [[nodiscard]] Color contrasted() const;

    [[nodiscard]] _FORCE_INLINE_ Color linear_interpolate(const Color &p_b, float p_t) const {

        Color res = *this;

        res.r += (p_t * (p_b.r - r));
        res.g += (p_t * (p_b.g - g));
        res.b += (p_t * (p_b.b - b));
        res.a += (p_t * (p_b.a - a));

        return res;
    }

    [[nodiscard]] constexpr Color darkened(float p_amount) const {
        return Color(
            r * (1.0f - p_amount),
            g * (1.0f - p_amount),
            b * (1.0f - p_amount)
        );
    }

    [[nodiscard]] _FORCE_INLINE_ Color lightened(float p_amount) const {
        return Color(
            r + (1.0f - r) * p_amount,
            g + (1.0f - g) * p_amount,
            b + (1.0f - b) * p_amount
        );
    }

    [[nodiscard]] _FORCE_INLINE_ uint32_t to_rgbe9995() const {

        const float pow2to9 = 512.0f;
        const float B = 15.0f;
        //const float Emax = 31.0f;
        const float N = 9.0f;

        float sharedexp = 65408.0f; //(( pow2to9  - 1.0f)/ pow2to9)*powf( 2.0f, 31.0f - 15.0f);

        float cRed = MAX(0.0f, MIN(sharedexp, r));
        float cGreen = MAX(0.0f, MIN(sharedexp, g));
        float cBlue = MAX(0.0f, MIN(sharedexp, b));

        float cMax = MAX(cRed, MAX(cGreen, cBlue));

        // expp = MAX(-B - 1, log2(maxc)) + 1 + B

        const float expp = MAX(-B - 1.0f, std::floor(Math::log(cMax) / float(Math_LN2))) + 1.0f + B;

        float sMax = (float)std::floor((cMax / Math::pow(2.0f, expp - B - N)) + 0.5f);

        float exps = expp + 1.0f;

        if (0.0f <= sMax && sMax < pow2to9) {
            exps = expp;
        }

        float sRed = Math::floor((cRed / std::pow(2.0f, exps - B - N)) + 0.5f);
        float sGreen = Math::floor((cGreen / std::pow(2.0f, exps - B - N)) + 0.5f);
        float sBlue = Math::floor((cBlue / std::pow(2.0f, exps - B - N)) + 0.5f);

        return (uint32_t(Math::fast_ftoi(sRed)) & 0x1FF) | ((uint32_t(Math::fast_ftoi(sGreen)) & 0x1FF) << 9) | ((uint32_t(Math::fast_ftoi(sBlue)) & 0x1FF) << 18) | ((uint32_t(Math::fast_ftoi(exps)) & 0x1F) << 27);
    }

    [[nodiscard]] _FORCE_INLINE_ Color blend(const Color &p_over) const {

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

    [[nodiscard]] constexpr Color to_linear() const {

        return Color(
                r < 0.04045f ? r * (1.0f / 12.92f) : Math::pow((r + 0.055f) * (1.0f / (1 + 0.055f)), 2.4f),
                g < 0.04045f ? g * (1.0f / 12.92f) : Math::pow((g + 0.055f) * (1.0f / (1 + 0.055f)), 2.4f),
                b < 0.04045f ? b * (1.0f / 12.92f) : Math::pow((b + 0.055f) * (1.0f / (1 + 0.055f)), 2.4f),
                a);
    }
    [[nodiscard]] constexpr Color to_srgb() const {

        return Color(
                r < 0.0031308f ? 12.92f * r : (1.0f + 0.055f) * Math::pow(r, 1.0f / 2.4f) - 0.055f,
                g < 0.0031308f ? 12.92f * g : (1.0f + 0.055f) * Math::pow(g, 1.0f / 2.4f) - 0.055f,
                b < 0.0031308f ? 12.92f * b : (1.0f + 0.055f) * Math::pow(b, 1.0f / 2.4f) - 0.055f, a);
    }

    static Color hex(uint32_t p_hex);
    static Color hex64(uint64_t p_hex);
    static Color html(se_string_view p_color);
    static bool html_is_valid(se_string_view p_color);
    static Color named(se_string_view p_name);
    [[nodiscard]] se_string to_html(bool p_alpha = true) const;
    [[nodiscard]] Color from_hsv(float p_h, float p_s, float p_v, float p_a) const;
    static Color from_rgbe9995(uint32_t p_rgbe);

    _FORCE_INLINE_ bool operator<(const Color &p_color) const; //used in set keys
    operator se_string() const;

    /**
     * No construct parameters, r=0, g=0, b=0. a=255
     */
    constexpr _FORCE_INLINE_ Color() : r(0),g(0),b(0),a(1.0f) {
    }

    /**
     * RGB / RGBA construct parameters. Alpha is optional, but defaults to 1.0
     */
    constexpr Color(float p_r, float p_g, float p_b, float p_a = 1.0) : r(p_r),g(p_g),b(p_b),a(p_a) {
    }
};

bool Color::operator<(const Color &p_color) const {

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
