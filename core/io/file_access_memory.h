/*************************************************************************/
/*  file_access_memory.h                                                 */
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
#include "EASTL/string_view.h"
class FileAccessMemory : public FileAccess {

    uint8_t *data;
    uint64_t length;
    mutable uint64_t pos;

    static FileAccess *create();

public:
    static void register_file(StringView p_name, const Vector<uint8_t> &p_data);
    static void cleanup();

    virtual Error open_custom(const uint8_t *p_data, uint64_t p_len); ///< open a file
    Error _open(StringView p_path, int p_mode_flags) override; ///< open a file
    void close() override; ///< close a file
    bool is_open() const override; ///< true when file is open

    void seek(uint64_t p_position) override; ///< seek to a given position
    void seek_end(int64_t p_position) override; ///< seek from the end of file
    uint64_t get_position() const override; ///< get position in the file
    uint64_t get_len() const override; ///< get size of the file

    bool eof_reached() const override; ///< reading passed EOF

    uint8_t get_8() const override; ///< get a byte

    uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override; ///< get an array of bytes

    Error get_error() const override; ///< get last error

    void flush() override;
    void store_8(uint8_t p_byte) override; ///< store a byte
    void store_buffer(const uint8_t *p_src, uint64_t p_length) override; ///< store an array of bytes

    bool file_exists(StringView p_name) override; ///< return true if a file exists

    uint64_t _get_modified_time(StringView /*p_file*/) override { return 0; }
    uint32_t _get_unix_permissions(StringView /*p_file*/) override { return 0; }
    Error _set_unix_permissions(StringView /*p_file*/, uint32_t /*p_permissions*/) override { return FAILED; }

    FileAccessMemory();
};
