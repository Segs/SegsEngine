/*************************************************************************/
/*  zip_io.h                                                             */
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

#include "core/os/file_access.h"

// Not directly used in this header, but assumed available in downstream users
// like platform/*/export/export.cpp. Could be fixed, but probably better to have
// thirdparty includes in as little headers as possible.
#include "thirdparty/minizip/unzip.h"
#include "thirdparty/minizip/zip.h"

GODOT_EXPORT void *zipio_open(void *data, const char *p_fname, int mode);
GODOT_EXPORT uLong zipio_read(void *data, void *fdata, void *buf, uLong size);
GODOT_EXPORT uLong zipio_write(voidpf opaque, voidpf stream, const void *buf, uLong size);

GODOT_EXPORT long zipio_tell(voidpf opaque, voidpf stream);
GODOT_EXPORT long zipio_seek(voidpf opaque, voidpf stream, uLong offset, int origin);

GODOT_EXPORT int zipio_close(voidpf opaque, voidpf stream);

GODOT_EXPORT int zipio_testerror(voidpf opaque, voidpf stream);

GODOT_EXPORT voidpf zipio_alloc(voidpf opaque, uInt items, uInt size);
GODOT_EXPORT void zipio_free(voidpf opaque, voidpf address);

GODOT_EXPORT zlib_filefunc_def zipio_create_io_from_file(FileAccess **p_file);
