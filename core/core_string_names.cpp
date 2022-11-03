/*************************************************************************/
/*  core_string_names.cpp                                                */
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

#include "core_string_names.h"

CoreStringNames *CoreStringNames::singleton = nullptr;

CoreStringNames::CoreStringNames() :
        _free("free"),
        changed("changed"),
        _meta("__meta__"),
        _script("script"),
        script_changed("script_changed"),
        ___pdcdata("___pdcdata"),
        __getvar("__getvar"),
        _iter_init("_iter_init"),
        _iter_next("_iter_next"),
        _iter_get("_iter_get"),
        get_phys_rid("get_phys_rid"),
        get_rid("get_rid"),
        _to_string("_to_string"),
        _sections_unfolded("_sections_unfolded"),
        _custom_features("_custom_features"),
        x("x"),
        y("y"),
        z("z"),
        w("w"),
        r("r"),
        g("g"),
        b("b"),
        a("a"),
        position("position"),
        size("size"),
        end("end"),
        basis("basis"),
        origin("origin"),
        normal("normal"),
        d("d"),
        h("h"),
        s("s"),
        v("v"),
        r8("r8"),
        g8("g8"),
        b8("b8"),
        a8("a8"),
        call("call"),
        emit("emit"),
        notification("notification")
{
}
