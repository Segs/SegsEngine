/*************************************************************************/
/*  core_string_names.cpp                                                */
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

#include "core_string_names.h"

CoreStringNames *CoreStringNames::singleton = nullptr;

CoreStringNames::CoreStringNames() :
		_free(StaticCString("free")),
		changed(StaticCString("changed")),
		_meta(StaticCString("__meta__")),
		_script(StaticCString("script")),
		script_changed(StaticCString("script_changed")),
		___pdcdata(StaticCString("___pdcdata")),
		__getvar(StaticCString("__getvar")),
		_iter_init(StaticCString("_iter_init")),
		_iter_next(StaticCString("_iter_next")),
		_iter_get(StaticCString("_iter_get")),
		get_rid(StaticCString("get_rid")),
		_to_string(StaticCString("_to_string")),
#ifdef TOOLS_ENABLED
		_sections_unfolded(StaticCString("_sections_unfolded")),
#endif
		_custom_features(StaticCString("_custom_features")),
		x(StaticCString("x")),
		y(StaticCString("y")),
		z(StaticCString("z")),
		w(StaticCString("w")),
		r(StaticCString("r")),
		g(StaticCString("g")),
		b(StaticCString("b")),
		a(StaticCString("a")),
		position(StaticCString("position")),
		size(StaticCString("size")),
		end(StaticCString("end")),
		basis(StaticCString("basis")),
		origin(StaticCString("origin")),
		normal(StaticCString("normal")),
		d(StaticCString("d")),
		h(StaticCString("h")),
		s(StaticCString("s")),
		v(StaticCString("v")),
		r8(StaticCString("r8")),
		g8(StaticCString("g8")),
		b8(StaticCString("b8")),
		a8(StaticCString("a8")) {
}
