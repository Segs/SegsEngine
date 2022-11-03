/*************************************************************************/
/*  file_access_buffered.h                                               */
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
#include "core/vector.h"
#include "core/string.h"

class FileAccessBuffered : public FileAccess {

public:
    enum {
        DEFAULT_CACHE_SIZE = 128 * 1024,
    };

private:
    int cache_size = DEFAULT_CACHE_SIZE;

    int cache_data_left() const;
    mutable Error last_error;

protected:
    Error set_error(Error p_error) const;

    mutable struct File {

        bool open;
        int size;
        int offset;
        String name;
        int access_flags;
    } file;

    mutable struct Cache {

        Vector<uint8_t> buffer;
        int offset;
    } cache;

    virtual int read_data_block(int p_offset, int p_size, uint8_t *p_dest = nullptr) const = 0;

    void set_cache_size(int p_size);
    int get_cache_size();

public:
    size_t get_position() const override; ///< get position in the file
    size_t get_len() const override; ///< get size of the file

    void seek(size_t p_position) override; ///< seek to a given position
    void seek_end(int64_t p_position = 0) override; ///< seek from the end of file

    bool eof_reached() const override;

    uint8_t get_8() const override;
    uint64_t get_buffer(uint8_t *p_dest, uint64_t p_length) const override; ///< get an array of bytes

    bool is_open() const override;

    Error get_error() const override;

    FileAccessBuffered() = default;
    ~FileAccessBuffered() override = default ;
};
