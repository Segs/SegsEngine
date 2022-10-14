/*************************************************************************/
/*  vector.h                                                             */
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
#include "core/forward_decls.h"
#include "core/error_macros.h"
#include "core/os/memory.h"
#include "core/sort_array.h"
#include "EASTL/vector.h"
#include "EASTL/fixed_vector.h"
#include "EASTL/span.h"

template<class T>
using Vector = eastl::vector<T,wrap_allocator>;
template<class T,int N,bool GROWING>
using FixedVector = eastl::fixed_vector<T,N,GROWING,wrap_allocator>;
template <typename T,size_t sz>
using Span = eastl::span<T,sz>;
class Variant;


#ifndef __MINGW32__
#ifdef GODOT_EXPORTS
extern template class EXPORT_TEMPLATE_DECL eastl::vector<class StringName,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECL eastl::vector<String,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECL eastl::vector<uint8_t,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECL eastl::vector<int,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECL eastl::vector<float,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECL eastl::vector<Variant,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECL eastl::vector<struct PropertyInfo,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECL eastl::vector<struct Vector2,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECL eastl::vector<struct Vector3,wrap_allocator>;
#endif
#endif
extern const Vector<struct Vector2> null_vec2_pvec;
extern const Vector<struct Vector3> null_vec3_pvec;
extern const Vector<String> null_string_pvec;
extern const Vector<int> null_int_pvec;
extern const Vector<float> null_float_pvec;
