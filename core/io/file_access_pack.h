/*************************************************************************/
/*  file_access_pack.h                                                   */
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

#include "core/list.h"
#include "core/map.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/string.h"
#include "core/string_utils.h"
#include "core/vector.h"
#include "core/plugin_interfaces/PackSourceInterface.h"
#include "core/set.h"
#include "core/vector.h"

#include <cassert>

// Godot's packed file magic header ("GDPC" in ASCII).
#define PACK_HEADER_MAGIC 0x43504447
// The current packed file format version number.
#define PACK_FORMAT_VERSION 1

class PackSourceInterface;

struct PackedDataFile {

    String pack;
    uint64_t offset; //if offset is ZERO, the file was ERASED
    uint64_t size;
    uint8_t md5[16];
    PackSourceInterface *src;
};

class GODOT_EXPORT PackedData {
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

        PathMD5(const FixedVector<uint8_t,16,false> &p_buf) {
            assert(p_buf.size()==16);
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
    void add_path(StringView pkg_path, StringView path, uint64_t ofs, uint64_t size, const uint8_t *p_md5, PackSourceInterface *p_src, bool p_replace_files); // for PackSource

    void set_disabled(bool p_disabled) { disabled = p_disabled; }
    _FORCE_INLINE_ bool is_disabled() const { return disabled; }

    static PackedData *get_singleton() { return singleton; }
    Error add_pack(StringView p_path, bool p_replace_files, StringView p_destination="");

    _FORCE_INLINE_ FileAccess *try_open_path(StringView p_path);
    _FORCE_INLINE_ bool has_path(StringView p_path);

    _FORCE_INLINE_ DirAccess *try_open_directory(StringView p_path);
    _FORCE_INLINE_ bool has_directory(StringView p_path);

    PackedData();
    ~PackedData();
};


FileAccess *PackedData::try_open_path(StringView p_path) {

    PathMD5 pmd5(StringUtils::md5_buffer(p_path));
    auto E = files.find(pmd5);
    if (E==files.end())
        return nullptr; //not found
    if (E->second.offset == 0)
        return nullptr; //was erased

    return E->second.src->get_file(p_path, &E->second);
}

bool PackedData::has_path(StringView p_path) {

    return files.contains(PathMD5(StringUtils::md5_buffer(p_path)));
}
bool PackedData::has_directory(StringView p_path) {

    DirAccessRef da = try_open_directory(p_path);
    return da==true;
}
class DirAccessPack : public DirAccess {

    PackedData::PackedDir *current;

    Vector<String> list_dirs;
    Vector<String> list_files;
    int m_dir_offset=0;
    int m_file_offset=0;
    bool cdir;

    PackedData::PackedDir *_find_dir(StringView p_dir) const;

public:
    Error list_dir_begin() override;
    String get_next() override;
    [[nodiscard]] bool current_is_dir() const override;
    [[nodiscard]] bool current_is_hidden() const override;
    void list_dir_end() override;

    int get_drive_count() override;
    String get_drive(int p_drive) override;

    Error change_dir(StringView p_dir) override;
    String get_current_dir() override;

    bool file_exists(StringView p_file) override;
    bool dir_exists(StringView p_dir) override;

    Error make_dir(StringView p_dir) override;

    Error rename(StringView p_from, StringView p_to) override;
    Error remove(StringView p_name) override;

    size_t get_space_left() override;

    String get_filesystem_type() const override;

    DirAccessPack();
    ~DirAccessPack() override;
};

DirAccess *PackedData::try_open_directory(StringView p_path) {

    DirAccess *da = memnew(DirAccessPack());
    if (da->change_dir(p_path) != OK) {
        memdelete(da);
        da = nullptr;
    }
    return da;
}
