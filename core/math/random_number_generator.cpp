/*************************************************************************/
/*  random_number_generator.cpp                                          */
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

#include "random_number_generator.h"
#include "core/method_bind.h"

IMPL_GDCLASS(RandomNumberGenerator)


void RandomNumberGenerator::_bind_methods() {
    SE_BIND_METHOD(RandomNumberGenerator,set_seed);
    SE_BIND_METHOD(RandomNumberGenerator,get_seed);

    SE_BIND_METHOD(RandomNumberGenerator,set_state);
    SE_BIND_METHOD(RandomNumberGenerator,get_state);

    SE_BIND_METHOD(RandomNumberGenerator,randi);
    SE_BIND_METHOD(RandomNumberGenerator,randf);
    MethodBinder::bind_method(D_METHOD("randfn", {"mean", "deviation"}), &RandomNumberGenerator::randfn, {DEFVAL(0.0), DEFVAL(1.0)});
    SE_BIND_METHOD(RandomNumberGenerator,randf_range);
    SE_BIND_METHOD(RandomNumberGenerator,randi_range);
    SE_BIND_METHOD(RandomNumberGenerator,randomize);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "seed"), "set_seed", "get_seed");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "state"), "set_state", "get_state");
    // Default values are non-deterministic, override for doc generation purposes.
    ADD_PROPERTY_DEFAULT("seed", 0);
    ADD_PROPERTY_DEFAULT("state", 0);
}
