/*************************************************************************/
/*  math_defs.h                                                          */
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
#include "core/configuration.h"
#include <stdint.h>

#define CMP_EPSILON 0.00001f
#define CMP_EPSILON2 (CMP_EPSILON * CMP_EPSILON)
#define CMP_NORMALIZE_TOLERANCE 0.000001f
#define CMP_POINT_IN_PLANE_EPSILON 0.00001f

template<typename T>
struct MathConsts;
template<>
struct MathConsts<float> {
    constexpr static float SQRT12 = 0.7071067811865475244008443621048490f;
    constexpr static float SQRT2 = 1.4142135623730950488016887242f;
    constexpr static float LN2 = 0.6931471805599453094172321215f;
    constexpr static float TAU = 6.2831853071795864769252867666f;
    constexpr static float PI = 3.1415926535897932384626433833f;
    constexpr static float E = 2.7182818284590452353602874714f;
};
template<>
struct MathConsts<double> {
    constexpr static double SQRT12 = 0.7071067811865475244008443621048490;
    constexpr static double SQRT2 = 1.4142135623730950488016887242;
    constexpr static double LN2 = 0.6931471805599453094172321215;
    constexpr static double TAU = 6.2831853071795864769252867666;
    constexpr static double PI = 3.1415926535897932384626433833;
    constexpr static double E = 2.7182818284590452353602874714;
};
//sqrt(2)/2.0
#define Math_SQRT12 MathConsts<float>::SQRT12
#define Math_SQRT2 MathConsts<float>::SQRT2
#define Math_LN2 MathConsts<float>::LN2
#define Math_TAU MathConsts<float>::TAU
#define Math_PI MathConsts<float>::PI
#define Math_E MathConsts<float>::E


#define Math_INF INFINITY
#define Math_NAN NAN

//this epsilon is for values related to a unit size (scalar or vector len)
#ifdef OPTION_PRECISE_MATH_CHECKS
#define UNIT_EPSILON 0.00001f
#else
//tolerate some more floating point error normally
#define UNIT_EPSILON 0.001f
#endif

#define USEC_TO_SEC(m_usec) ((m_usec) / 1000000.0f)

enum ClockDirection : int8_t {

    CLOCKWISE,
    COUNTERCLOCKWISE
};

enum Orientation : int8_t {

    HORIZONTAL,
    VERTICAL
};

enum HAlign : int8_t {

    HALIGN_LEFT,
    HALIGN_CENTER,
    HALIGN_RIGHT
};

enum VAlign : int8_t {

    VALIGN_TOP,
    VALIGN_CENTER,
    VALIGN_BOTTOM
};

enum class Margin : int8_t {

    Left=0, //!< Left margin, usually used for [Control] or [StyleBox]-derived classes.
    Top,
    Right,
    Bottom,
    Max
};

enum Corner : int8_t {

    CORNER_TOP_LEFT,
    CORNER_TOP_RIGHT,
    CORNER_BOTTOM_RIGHT,
    CORNER_BOTTOM_LEFT
};

using real_t = float;
