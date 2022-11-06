/*************************************************************************/
/*  random_pcg.cpp                                                       */
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

#include "random_pcg.h"

#include "thirdparty/misc/pcg.h"
#include "core/os/os.h"

#include <cmath>

RandomPCG::RandomPCG(uint64_t p_seed, uint64_t p_inc) :
        pcg(),
        current_inc(p_inc) {
    seed(p_seed);
}

void RandomPCG::seed(uint64_t p_seed) {
    current_seed = p_seed;
    pcg32_srandom_r((pcg32_random_t*)&pcg, current_seed, current_inc);
}

void RandomPCG::randomize() {
    seed((OS::get_singleton()->get_unix_time() + OS::get_singleton()->get_ticks_usec()) * pcg.state + PCG_DEFAULT_INC_64);
}

uint32_t RandomPCG::rand() {
    current_seed = pcg.state;
    return pcg32_random_r((pcg32_random_t*)&pcg);
}

uint32_t RandomPCG::rand(uint32_t bounds) {
    current_seed = pcg.state;
    return pcg32_boundedrand_r((pcg32_random_t*)&pcg, bounds);
}

double RandomPCG::randd() {
#if defined(CLZ32)
    uint32_t proto_exp_offset = rand();
    if (unlikely(proto_exp_offset == 0)) {
        return 0;
    }
    uint64_t significand = (((uint64_t)rand()) << 32) | rand() | 0x8000000000000001U;
    return LDEXP((double)significand, -64 - CLZ32(proto_exp_offset));
#else
#pragma message("RandomPCG::randd - intrinsic clz is not available, falling back to bit truncation")
        return (double)(((((uint64_t)rand()) << 32) | rand()) & 0x1FFFFFFFFFFFFFU) / (double)0x1FFFFFFFFFFFFFU;
#endif
}

float RandomPCG::randf() {
#if defined(CLZ32)
    uint32_t proto_exp_offset = rand();
    if (unlikely(proto_exp_offset == 0)) {
        return 0;
    }
    return LDEXPF((float)(rand() | 0x80000001), -32 - CLZ32(proto_exp_offset));
#else
#pragma message("RandomPCG::randf - intrinsic clz is not available, falling back to bit truncation")
        return (float)(rand() & 0xFFFFFF) / (float)0xFFFFFF;
#endif
}

double RandomPCG::randfn(double p_mean, double p_deviation) {
    return p_mean + p_deviation * (std::cos(MathConsts<double>::TAU * randd()) *
                                          std::sqrt(-2.0 * std::log(randd()))); // Box-Muller transform
}

float RandomPCG::randfn(float p_mean, float p_deviation) {
    return p_mean + p_deviation * (std::cos(float(Math_TAU) * randf()) *
                                          std::sqrt(-2.0f * std::log(randf()))); // Box-Muller transform
}

double RandomPCG::random(double p_from, double p_to) {
    return randd() * (p_to - p_from) + p_from;
}

float RandomPCG::random(float p_from, float p_to) {
    return randf() * (p_to - p_from) + p_from;
}
