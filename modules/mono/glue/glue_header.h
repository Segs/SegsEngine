/*************************************************************************/
/*  glue_header.h                                                        */
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
#include "base_object_glue.h"
#include "collections_glue.h"
#include "gd_glue.h"
#include "nodepath_glue.h"
#include "rid_glue.h"
#include "string_glue.h"

// Used by the generated glue

#include "core/array.h"
#include "core/class_db.h"
#include "core/dictionary.h"
#include "core/engine.h"
#include "core/method_bind.h"
#include "core/node_path.h"
#include "core/object.h"
#include "core/reference.h"
#include "core/typedefs.h"
#include "core/ustring.h"

#include "../mono_gd/gd_mono_class.h"
#include "../mono_gd/gd_mono_internals.h"
#include "../mono_gd/gd_mono_utils.h"

#define GODOTSHARP_INSTANCE_OBJECT(m_instance, m_type) \
    static ClassDB_ClassInfo *ci = nullptr;              \
    if (!ci) {                                         \
        ci = &ClassDB::classes.find(m_type)->second;    \
    }                                                  \
    Object *m_instance = ci->creation_func();

#include "arguments_vector.h"
