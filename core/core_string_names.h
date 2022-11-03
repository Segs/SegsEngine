/*************************************************************************/
/*  core_string_names.h                                                  */
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

#include "core/string_name.h"
#include "core/os/memory.h"

class CoreStringNames {

    friend void register_core_types();
    friend void unregister_core_types();

    static void create() { singleton = memnew(CoreStringNames); }
    static void free() {
        memdelete(singleton);
        singleton = nullptr;
    }

    CoreStringNames();

public:
    _FORCE_INLINE_ static CoreStringNames *get_singleton() { return singleton; }

    static CoreStringNames *singleton;

    const StringName _free;
    const StringName changed;
    const StringName _meta;
    const StringName _script;
    const StringName script_changed;
    const StringName ___pdcdata;
    const StringName __getvar;
    const StringName _iter_init;
    const StringName _iter_next;
    const StringName _iter_get;
    const StringName get_phys_rid;
    const StringName get_rid;
    const StringName _to_string;
    const StringName _sections_unfolded;
    const StringName _custom_features;

    const StringName x;
    const StringName y;
    const StringName z;
    const StringName w;
    const StringName r;
    const StringName g;
    const StringName b;
    const StringName a;
    const StringName position;
    const StringName size;
    const StringName end;
    const StringName basis;
    const StringName origin;
    const StringName normal;
    const StringName d;
    const StringName h;
    const StringName s;
    const StringName v;
    const StringName r8;
    const StringName g8;
    const StringName b8;
    const StringName a8;

    const StringName call;
    const StringName emit;
    const StringName notification;
};
