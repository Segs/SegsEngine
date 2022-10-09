/*************************************************************************/
/*  dir_access_windows.h                                                 */
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

#ifndef DIR_ACCESS_WINDOWS_H
#define DIR_ACCESS_WINDOWS_H

#ifdef WINDOWS_ENABLED

#include "core/os/dir_access.h"
#include "core/string.h"

/**
    @author Juan Linietsky <reduz@gmail.com>
*/

struct DirAccessWindowsPrivate;

class DirAccessWindows : public DirAccess {

    enum {
        MAX_DRIVES = 26
    };

    DirAccessWindowsPrivate *p;
    /* Windows stuff */

    char drives[MAX_DRIVES]; // a-z:
    int drive_count = 0;

    String current_dir;

    bool _cisdir;
    bool _cishidden;

public:
    Error list_dir_begin() override; ///< This starts dir listing
    String get_next() override;
    bool current_is_dir() const override;
    bool current_is_hidden() const override;
    void list_dir_end() override; ///<

    int get_drive_count() override;
    String get_drive(int p_drive) override;

    Error change_dir(StringView p_dir) override; ///< can be relative or absolute, return false on success
    String get_current_dir() override; ///< return current dir location
    String get_current_dir_without_drive() override;

    bool file_exists(StringView p_file) override;
    bool dir_exists(StringView p_dir) override;

    Error make_dir(StringView p_dir) override;

    Error rename(StringView p_path, StringView p_new_path) override;
    Error remove(StringView p_path) override;

    //virtual FileType get_file_type() const;
    virtual bool is_link(StringView p_file) { return false; };
    virtual String read_link(StringView p_file) { return String(p_file); };
    virtual Error create_link(StringView p_source, StringView p_target) { return FAILED; };

    uint64_t get_space_left();

    String get_filesystem_type() const override;

    DirAccessWindows();
    ~DirAccessWindows();
};

#endif //WINDOWS_ENABLED

#endif
