/*************************************************************************/
/*  dir_access_unix.h                                                    */
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

#if defined(UNIX_ENABLED) || defined(LIBC_FILEIO_ENABLED)

#include "core/os/dir_access.h"
#include "core/se_string.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

class DirAccessUnix : public DirAccess {

    DIR *dir_stream;

    static DirAccess *create_fs();

    se_string current_dir;
    bool _cisdir;
    bool _cishidden;

protected:
    virtual se_string fix_unicode_name(const char *p_name) const { return p_name; }

public:
    Error list_dir_begin() override; ///< This starts dir listing
    se_string get_next() override;
    bool current_is_dir() const override;
    bool current_is_hidden() const override;

    void list_dir_end() override; ///<

    int get_drive_count() override;
    se_string get_drive(int p_drive) override;

    Error change_dir(se_string_view p_dir) override; ///< can be relative or absolute, return false on success
    se_string get_current_dir() override; ///< return current dir location
    Error make_dir(se_string_view p_dir) override;

    bool file_exists(se_string_view p_file) override;
    bool dir_exists(se_string_view p_dir) override;

    virtual uint64_t get_modified_time(se_string_view p_file);

    Error rename(se_string_view p_path, se_string_view p_new_path) override;
    Error remove(se_string_view p_path) override;

    size_t get_space_left() override;

    se_string get_filesystem_type() const override;

    DirAccessUnix();
    ~DirAccessUnix() override;
};

#endif //UNIX ENABLED
