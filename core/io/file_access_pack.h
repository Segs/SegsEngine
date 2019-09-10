/*************************************************************************/
/*  file_access_pack.h                                                   */
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

#pragma once

#include "core/list.h"
#include "core/map.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/ustring.h"
#include "core/plugin_interfaces/PackSourceInterface.h"

class PackSourceInterface;

struct PackedDataFile {

    String pack;
    uint64_t offset; //if offset is ZERO, the file was ERASED
    uint64_t size;
    uint8_t md5[16];
    PackSourceInterface *src;
};

class PackedData {
    friend class FileAccessPack;
    friend class DirAccessPack;
    friend class PackSourceInterface;

public:

private:
    struct PackedDir {
        PackedDir *parent;
        String name;
        Map<String, PackedDir *> subdirs;
        Set<String> files;
    };

    struct PathMD5 {
        uint64_t a;
        uint64_t b;
        bool operator<(PathMD5 p_md5) const {

            if (p_md5.a == a) {
                return b < p_md5.b;
            }
            return a < p_md5.a;
        }

        bool operator==(PathMD5 p_md5) const {
            return a == p_md5.a && b == p_md5.b;
        }

        PathMD5() {
            a = b = 0;
        }

        PathMD5(const Vector<uint8_t> &p_buf) {
            assert(p_buf.size()>=16);
            a = *((uint64_t *)&p_buf[0]);
            b = *((uint64_t *)&p_buf[8]);
        }
    };

    Map<PathMD5, PackedDataFile> files;

    Vector<PackSourceInterface *> sources;

    PackedDir *root;
    //Map<String,PackedDir*> dirs;

    static PackedData *singleton;
    bool disabled;

    void _free_packed_dirs(PackedDir *p_dir);

public:
    void add_pack_source(PackSourceInterface *p_source);
    void remove_pack_source(PackSourceInterface *p_source);
    void add_path(const String &pkg_path, const String &path, uint64_t ofs, uint64_t size, const uint8_t *p_md5, PackSourceInterface *p_src); // for PackSource

    void set_disabled(bool p_disabled) { disabled = p_disabled; }
    _FORCE_INLINE_ bool is_disabled() const { return disabled; }

    static PackedData *get_singleton() { return singleton; }
    Error add_pack(const String &p_path);

    _FORCE_INLINE_ FileAccess *try_open_path(const String &p_path);
    _FORCE_INLINE_ bool has_path(const String &p_path);

    PackedData();
    ~PackedData();
};

class PackedSourcePCK : public PackSourceInterface {

public:
    bool try_open_pack(const String &p_path) override;
    FileAccess *get_file(const String &p_path, PackedDataFile *p_file) override;
};

class FileAccessPack : public FileAccess {

    PackedDataFile pf;

    mutable size_t pos;
    mutable bool eof;

    FileAccess *f;
    Error _open(const String &p_path, int p_mode_flags) override;
    uint64_t _get_modified_time(const String &p_file) override { return 0; }
    uint32_t _get_unix_permissions(const String &p_file) override { return 0; }
    Error _set_unix_permissions(const String &p_file, uint32_t p_permissions) override { return FAILED; }

public:
    void close() override;
    bool is_open() const override;

    void seek(size_t p_position) override;
    void seek_end(int64_t p_position = 0) override;
    size_t get_position() const override;
    size_t get_len() const override;

    bool eof_reached() const override;

    uint8_t get_8() const override;

    int get_buffer(uint8_t *p_dst, int p_length) const override;

    void set_endian_swap(bool p_swap) override;

    Error get_error() const override;

    void flush() override;
    void store_8(uint8_t p_dest) override;

    void store_buffer(const uint8_t *p_src, int p_length) override;

    bool file_exists(const String &p_name) override;

    FileAccessPack(const String &p_path, const PackedDataFile &p_file);
    ~FileAccessPack() override;
};

FileAccess *PackedData::try_open_path(const String &p_path) {

    PathMD5 pmd5(StringUtils::md5_buffer(p_path));
    Map<PathMD5, PackedDataFile>::Element *E = files.find(pmd5);
    if (!E)
        return nullptr; //not found
    if (E->get().offset == 0)
        return nullptr; //was erased

    return E->get().src->get_file(p_path, &E->get());
}

bool PackedData::has_path(const String &p_path) {

    return files.has(PathMD5(StringUtils::md5_buffer(p_path)));
}

class DirAccessPack : public DirAccess {

    PackedData::PackedDir *current;

    List<String> list_dirs;
    List<String> list_files;
    bool cdir;

public:
    Error list_dir_begin() override;
    String get_next() override;
    bool current_is_dir() const override;
    bool current_is_hidden() const override;
    void list_dir_end() override;

    int get_drive_count() override;
    String get_drive(int p_drive) override;

    Error change_dir(String p_dir) override;
    String get_current_dir() override;

    bool file_exists(String p_file) override;
    bool dir_exists(String p_dir) override;

    Error make_dir(String p_dir) override;

    Error rename(String p_from, String p_to) override;
    Error remove(String p_name) override;

    size_t get_space_left() override;

    String get_filesystem_type() const override;

    DirAccessPack();
    ~DirAccessPack() override;
};
