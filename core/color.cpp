/*************************************************************************/
/*  color.cpp                                                            */
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

#include "color.h"

#include "core/color_names.inc"
#include "core/error_macros.h"
#include "core/map.h"
#include "core/math/math_funcs.h"
#include "core/string.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"

namespace {

static constexpr char vals[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
void _to_hex(float p_val, char *tgt) {
    int v = int(p_val * 255);
    v = CLAMP(v, 0, 255);
    tgt[1] = vals[(v & 0xF)];
    tgt[0] = vals[((v >> 4) & 0xF)];
}
} // end of anonymous namespace

uint32_t Color::to_argb32() const {
    uint32_t c = (uint8_t)Math::round(a * 255);
    c <<= 8;
    c |= (uint8_t)Math::round(r * 255);
    c <<= 8;
    c |= (uint8_t)Math::round(g * 255);
    c <<= 8;
    c |= (uint8_t)Math::round(b * 255);

    return c;
}

uint32_t Color::to_abgr32() const {
    uint32_t c = (uint8_t)Math::round(a * 255);
    c <<= 8;
    c |= (uint8_t)Math::round(b * 255);
    c <<= 8;
    c |= (uint8_t)Math::round(g * 255);
    c <<= 8;
    c |= (uint8_t)Math::round(r * 255);

    return c;
}

uint32_t Color::to_rgba32() const {
    uint32_t c = (uint8_t)Math::round(r * 255);
    c <<= 8;
    c |= (uint8_t)Math::round(g * 255);
    c <<= 8;
    c |= (uint8_t)Math::round(b * 255);
    c <<= 8;
    c |= (uint8_t)Math::round(a * 255);

    return c;
}

uint64_t Color::to_abgr64() const {
    uint64_t c = (uint16_t)Math::round(a * 65535);
    c <<= 16;
    c |= (uint16_t)Math::round(b * 65535);
    c <<= 16;
    c |= (uint16_t)Math::round(g * 65535);
    c <<= 16;
    c |= (uint16_t)Math::round(r * 65535);

    return c;
}

uint64_t Color::to_argb64() const {
    uint64_t c = (uint16_t)Math::round(a * 65535);
    c <<= 16;
    c |= (uint16_t)Math::round(r * 65535);
    c <<= 16;
    c |= (uint16_t)Math::round(g * 65535);
    c <<= 16;
    c |= (uint16_t)Math::round(b * 65535);

    return c;
}

uint64_t Color::to_rgba64() const {
    uint64_t c = (uint16_t)Math::round(r * 65535);
    c <<= 16;
    c |= (uint16_t)Math::round(g * 65535);
    c <<= 16;
    c |= (uint16_t)Math::round(b * 65535);
    c <<= 16;
    c |= (uint16_t)Math::round(a * 65535);

    return c;
}

float Color::get_h() const {
    float min = MIN(r, g);
    min = MIN(min, b);
    float max = M_MAX(r, g);
    max = M_MAX(max, b);

    float delta = max - min;

    if (delta == 0.0f) {
        return 0;
    }

    float h;
    if (r == max) {
        h = (g - b) / delta; // between yellow & magenta
    } else if (g == max) {
        h = 2 + (b - r) / delta; // between cyan & yellow
    } else {
        h = 4 + (r - g) / delta; // between magenta & cyan
    }

    h /= 6.0f;
    if (h < 0) {
        h += 1.0f;
    }

    return h;
}

float Color::get_s() const {
    float min = MIN(r, g);
    min = MIN(min, b);
    float max = M_MAX(r, g);
    max = M_MAX(max, b);

    float delta = max - min;

    return (max != 0.0f) ? (delta / max) : 0;
}

float Color::get_v() const {
    float max = M_MAX(r, g);
    max = M_MAX(max, b);
    return max;
}

void Color::set_hsv(float p_h, float p_s, float p_v, float p_alpha) {
    int i;
    float f, p, q, t;
    a = p_alpha;

    if (p_s == 0) {
        // acp_hromatic (grey)
        r = g = b = p_v;
        return;
    }

    p_h *= 6.0f;
    p_h = Math::fmod(p_h, 6);
    i = Math::floor(p_h);

    f = p_h - i;
    p = p_v * (1 - p_s);
    q = p_v * (1 - p_s * f);
    t = p_v * (1 - p_s * (1 - f));

    switch (i) {
        case 0: // Red is the dominant color
            r = p_v;
            g = t;
            b = p;
            break;
        case 1: // Green is the dominant color
            r = q;
            g = p_v;
            b = p;
            break;
        case 2:
            r = p;
            g = p_v;
            b = t;
            break;
        case 3: // Blue is the dominant color
            r = p;
            g = q;
            b = p_v;
            break;
        case 4:
            r = t;
            g = p;
            b = p_v;
            break;
        default: // (5) Red is the dominant color
            r = p_v;
            g = p;
            b = q;
            break;
    }
}

void Color::contrast() {
    r = Math::fmod(r + 0.5f, 1.0f);
    g = Math::fmod(g + 0.5f, 1.0f);
    b = Math::fmod(b + 0.5f, 1.0f);
}

Color Color::hex(uint32_t p_hex) {
    float a = (p_hex & 0xFF) / 255.0f;
    p_hex >>= 8;
    float b = (p_hex & 0xFF) / 255.0f;
    p_hex >>= 8;
    float g = (p_hex & 0xFF) / 255.0f;
    p_hex >>= 8;
    float r = (p_hex & 0xFF) / 255.0f;

    return Color(r, g, b, a);
}

Color Color::hex64(uint64_t p_hex) {
    float a = (p_hex & 0xFFFF) / 65535.0f;
    p_hex >>= 16;
    float b = (p_hex & 0xFFFF) / 65535.0f;
    p_hex >>= 16;
    float g = (p_hex & 0xFFFF) / 65535.0f;
    p_hex >>= 16;
    float r = (p_hex & 0xFFFF) / 65535.0f;

    return Color(r, g, b, a);
}

Color Color::from_rgbe9995(uint32_t p_rgbe) {
    float r = p_rgbe & 0x1ff;
    float g = (p_rgbe >> 9) & 0x1ff;
    float b = (p_rgbe >> 18) & 0x1ff;
    float e = (p_rgbe >> 27);
    float m = Math::pow(2, e - 15.0f - 9.0f);

    float rd = r * m;
    float gd = g * m;
    float bd = b * m;

    return Color(rd, gd, bd, 1.0f);
}

static float _parse_col(StringView p_str, int p_ofs) {
    int ig = 0;

    for (int i = 0; i < 2; i++) {
        char c = p_str[i + p_ofs];
        int v = 0;

        if (isdigit(c)) {
            v = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            v = c - 'a';
            v += 10;
        } else if (c >= 'A' && c <= 'F') {
            v = c - 'A';
            v += 10;
        } else {
            return -1;
        }

        if (i == 0) {
            ig += v * 16;
        } else {
            ig += v;
        }
    }

    return ig;
}

Color Color::contrasted() const {
    Color c = *this;
    c.contrast();
    return c;
}

[[nodiscard]] uint32_t Color::to_rgbe9995() const {
    const float pow2to9 = 512.0f;
    const float B = 15.0f;
    // const float Emax = 31.0f;
    const float N = 9.0f;

    float sharedexp = 65408.0f; //(( pow2to9  - 1.0f)/ pow2to9)*powf( 2.0f, 31.0f - 15.0f);

    float cRed = M_MAX(0.0f, MIN(sharedexp, r));
    float cGreen = M_MAX(0.0f, MIN(sharedexp, g));
    float cBlue = M_MAX(0.0f, MIN(sharedexp, b));

    float cMax = M_MAX(cRed, M_MAX(cGreen, cBlue));

    // expp = M_MAX(-B - 1, log2(maxc)) + 1 + B

    const float expp = M_MAX(-B - 1.0f, std::floor(Math::log(cMax) / float(Math_LN2))) + 1.0f + B;

    float sMax = (float)std::floor((cMax / Math::pow(2.0f, expp - B - N)) + 0.5f);

    float exps = expp + 1.0f;

    if (0.0f <= sMax && sMax < pow2to9) {
        exps = expp;
    }

    float sRed = Math::floor((cRed / std::pow(2.0f, exps - B - N)) + 0.5f);
    float sGreen = Math::floor((cGreen / std::pow(2.0f, exps - B - N)) + 0.5f);
    float sBlue = Math::floor((cBlue / std::pow(2.0f, exps - B - N)) + 0.5f);

    return (uint32_t(Math::fast_ftoi(sRed)) & 0x1FF) | ((uint32_t(Math::fast_ftoi(sGreen)) & 0x1FF) << 9) |
           ((uint32_t(Math::fast_ftoi(sBlue)) & 0x1FF) << 18) | ((uint32_t(Math::fast_ftoi(exps)) & 0x1F) << 27);
}

Color Color::html(StringView p_color) {
    const String errcode("Invalid color code: ");
    String exp_color;
    if (p_color.length() == 0) {
        return Color();
    }
    if (p_color[0] == '#') {
        p_color = p_color.substr(1);
    }
    if (p_color.length() == 3 || p_color.length() == 4) {
        for (char i : p_color) {
            exp_color += i;
            exp_color += i;
        }
        p_color = exp_color;
    }

    bool alpha = false;

    if (p_color.length() == 8) {
        alpha = true;
    } else if (p_color.length() == 6) {
        alpha = false;
    } else {
        ERR_FAIL_V_MSG(Color(), errcode + p_color + ".");
    }

    int r = _parse_col(p_color, 0);
    ERR_FAIL_COND_V_MSG(r < 0, Color(), errcode + p_color + ".");
    int g = _parse_col(p_color, 2);
    ERR_FAIL_COND_V_MSG(g < 0, Color(), errcode + p_color + ".");
    int b = _parse_col(p_color, 4);
    ERR_FAIL_COND_V_MSG(b < 0, Color(), errcode + p_color + ".");
    int a = 255;
    if (alpha) {
        a = _parse_col(p_color, 6);
        ERR_FAIL_COND_V_MSG(a < 0, Color(), errcode + p_color + ".");
    }
    return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}
bool Color::html_is_valid(StringView p_color) {
    StringView color(p_color);

    if (color.empty()) {
        return false;
    }
    if (color[0] == '#') {
        color = color.substr(1);
    }

    bool alpha;

    if (color.length() == 8) {
        alpha = true;
    } else if (color.length() == 6) {
        alpha = false;
    } else {
        return false;
    }

    int r = _parse_col(color, 0);
    if (r < 0) {
        return false;
    }
    int g = _parse_col(color, 2);
    if (g < 0) {
        return false;
    }
    int b = _parse_col(color, 4);
    if (b < 0) {
        return false;
    }
    if (alpha) {
        int a = _parse_col(color, 6);
        if (a < 0) {
            return false;
        }
    }
    return true;
}

Color Color::named(StringView p_name) {
    String name(p_name);
    // Normalize name
    name = StringUtils::replace(name, " ", "");
    name = StringUtils::replace(name, "-", "");
    name = StringUtils::replace(name, "_", "");
    name = StringUtils::replace(name, "'", "");
    name = StringUtils::replace(name, ".", "");
    name = StringUtils::to_lower(name);

    const Map<const char *, Color>::iterator color = _named_colors.find(name.data());
    ERR_FAIL_COND_V_MSG(color == _named_colors.end(), Color(), "Invalid color name: " + String(p_name) + ".");
    return color->second;
}

String Color::to_html(bool p_alpha) const {
    String txt;
    txt.resize(p_alpha ? 8 : 6);
    _to_hex(r, txt.data());
    _to_hex(g, txt.data() + 2);
    _to_hex(b, txt.data() + 4);
    if (p_alpha) {
        _to_hex(a, txt.data() + 6);
    }
    return txt;
}

Color Color::from_hsv(float p_h, float p_s, float p_v, float p_a) {
    Color c;
    c.set_hsv(p_h, p_s, p_v, p_a);
    return c;
}

Color::operator String() const {
    return rtos(r) + ", " + rtos(g) + ", " + rtos(b) + ", " + rtos(a);
}

void Color::operator*=(Color p_color) {
    r = r * p_color.r;
    g = g * p_color.g;
    b = b * p_color.b;
    a = a * p_color.a;
}

void Color::operator*=(real_t rvalue) {
    r = r * rvalue;
    g = g * rvalue;
    b = b * rvalue;
    a = a * rvalue;
}

Color Color::operator/(Color p_color) const {
    return Color(r / p_color.r, g / p_color.g, b / p_color.b, a / p_color.a);
}

Color Color::operator/(real_t rvalue) const {
    return Color(r / rvalue, g / rvalue, b / rvalue, a / rvalue);
}

void Color::operator/=(Color p_color) {
    r = r / p_color.r;
    g = g / p_color.g;
    b = b / p_color.b;
    a = a / p_color.a;
}

void Color::operator/=(real_t rvalue) {
    if (rvalue == 0.0f) {
        r = 1.0f;
        g = 1.0f;
        b = 1.0f;
        a = 1.0f;
    } else {
        r = r / rvalue;
        g = g / rvalue;
        b = b / rvalue;
        a = a / rvalue;
    }
}

bool Color::is_equal_approx(Color p_color) const {
    return Math::is_equal_approx(r, p_color.r) && Math::is_equal_approx(g, p_color.g) && Math::is_equal_approx(b, p_color.b) && Math::is_equal_approx(a, p_color.a);
}
