/*************************************************************************/
/*  random_pcg.h                                                         */
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
#include "core/typedefs.h"
#include "core/math/math_defs.h"

#include <cmath>

#if defined(__GNUC__)
#define CLZ32(x) __builtin_clz(x)
#elif defined(_MSC_VER)
#include <intrin.h>
static int __bsr_clz32(uint32_t x) {
    unsigned long index;
    _BitScanReverse(&index, x);
    return 31 - index;
}
#define CLZ32(x) __bsr_clz32(x)
#else
#endif

#if defined(__GNUC__)
#define LDEXP(s, e) __builtin_ldexp(s, e)
#define LDEXPF(s, e) __builtin_ldexpf(s, e)
#else
#include <math.h>
#define LDEXP(s, e) ldexp(s, e)
#define LDEXPF(s, e) ldexp(s, e)
#endif

class RandomPCG {
    typedef struct { uint64_t state;  uint64_t inc; } random_state;
    random_state pcg;
    uint64_t current_seed; // seed with this to get the same state
    uint64_t current_inc;

public:
    static const uint64_t DEFAULT_SEED = 12047754176567800795U;
    static const uint64_t DEFAULT_INC = 1442695040888963407ULL; //PCG_DEFAULT_INC_64

    RandomPCG(uint64_t p_seed = DEFAULT_SEED, uint64_t p_inc = DEFAULT_INC);

    void seed(uint64_t p_seed);
    uint64_t get_seed() const { return current_seed; }

    void randomize();
    uint32_t rand();

    // Obtaining floating point numbers in [0, 1] range with "good enough" uniformity.
    // These functions sample the output of rand() as the fraction part of an infinite binary number,
    // with some tricks applied to reduce ops and branching:
    // 1. Instead of shifting to the first 1 and connecting random bits, we simply set the MSB and LSB to 1.
    //    Provided that the RNG is actually uniform bit by bit, this should have the exact same effect.
    // 2. In order to compensate for exponent info loss, we count zeros from another random number,
    //    and just add that to the initial offset.
    //    This has the same probability as counting and shifting an actual bit stream: 2^-n for n zeroes.
    // For all numbers above 2^-96 (2^-64 for floats), the functions should be uniform.
    // However, all numbers below that threshold are floored to 0.
    // The thresholds are chosen to minimize rand() calls while keeping the numbers within a totally subjective quality standard.
    // If clz or ldexp isn't available, fall back to bit truncation for performance, sacrificing uniformity.
    double randd();
    float randf();

    double randfn(double p_mean, double p_deviation) {
        return p_mean + p_deviation * (std::cos(MathConsts<double>::TAU * randd()) *
                                       std::sqrt(-2.0 * std::log(randd()))); // Box-Muller transform
    }
    float randfn(float p_mean, float p_deviation) {
        return p_mean + p_deviation * (std::cos(float(Math_TAU) * randf()) *
                                       std::sqrt(-2.0f * std::log(randf()))); // Box-Muller transform
    }

    double random(double p_from, double p_to);
    float random(float p_from, float p_to);
    real_t random(int p_from, int p_to) { return (real_t)random((real_t)p_from, (real_t)p_to); }
};
