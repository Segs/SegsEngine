/*************************************************************************/
/*  file_access_pack.cpp                                                 */
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

#include "file_access_pack.h"

#include "core/version.h"

#include <cstdio>

Error PackedData::add_pack(se_string_view p_path, bool p_replace_files) {

    for (auto & source : sources) {

        if (source->try_open_pack(p_path, p_replace_files)) {

            return OK;
        }
    }

    return ERR_FILE_UNRECOGNIZED;
}

void PackedData::add_path(se_string_view pkg_path, se_string_view path, uint64_t ofs, uint64_t size, const uint8_t *p_md5, PackSourceInterface *p_src, bool p_replace_files) {

    PathMD5 pmd5(StringUtils::md5_buffer(path));
    //printf("adding path %ls, %lli, %lli\n", path.c_str(), pmd5.a, pmd5.b);

    bool exists = files.contains(pmd5);

    PackedDataFile pf;
    pf.pack = pkg_path;
    pf.offset = ofs;
    pf.size = size;
    for (int i = 0; i < 16; i++)
        pf.md5[i] = p_md5[i];
    pf.src = p_src;

    if (!exists || p_replace_files)
        files[pmd5] = pf;

    if (!exists) {
        //search for dir
        String p = StringUtils::replace_first(path,"res://", "");
        PackedDir *cd = root;

        if (StringUtils::contains(p,'/')) { //in a subdir

            PODVector<se_string_view> ds = StringUtils::split(PathUtils::get_base_dir(p),'/');

            for (se_string_view sv : ds) {
                auto iter =  cd->subdirs.find_as<se_string_view>(sv);
                if (iter==cd->subdirs.end()) {

                    PackedDir *pd = memnew(PackedDir);
                    pd->name = sv;
                    pd->parent = cd;
                    cd->subdirs[pd->name] = pd;
                    cd = pd;
                } else {
                    cd = iter->second;
                }
            }
        }
        se_string_view filename = PathUtils::get_file(path);
        // Don't add as a file if the path points to a directory
        if (!filename.empty()) {
            cd->files.insert(filename);
        }
    }
}

void PackedData::add_pack_source(PackSourceInterface *p_source) {

    if (p_source != nullptr) {
        sources.push_back(p_source);
    }
}
/**
 * @brief PackedData::remove_pack_source will remove a source of pack files from available list.
 * @param p_source will be removed from the internal list, but will not be freed.
 */
void PackedData::remove_pack_source(PackSourceInterface *p_source)
{
    if (p_source != nullptr) {
        sources.erase_first(p_source);
    }

};

PackedData *PackedData::singleton = nullptr;

PackedData::PackedData() {

    singleton = this;
    root = memnew(PackedDir);
    root->parent = nullptr;
    disabled = false;
}

void PackedData::_free_packed_dirs(PackedDir *p_dir) {

    for (eastl::pair<const String, PackedDir *> &E : p_dir->subdirs)
        _free_packed_dirs(E.second);
    memdelete(p_dir);
}

PackedData::~PackedData() {

    //TODO: inform all sources that PackedData interface is being deleted ?
    sources.clear();
    _free_packed_dirs(root);
}


//////////////////////////////////////////////////////////////////////////////////
// DIR ACCESS
//////////////////////////////////////////////////////////////////////////////////

Error DirAccessPack::list_dir_begin() {

    list_dirs.clear();
    list_files.clear();
    list_dirs.reserve(current->subdirs.size());
    list_files.reserve(current->files.size());
    m_dir_offset = 0;
    m_file_offset = 0;
    for (eastl::pair<const String, PackedData::PackedDir *> &E : current->subdirs) {

        list_dirs.emplace_back(E.first);
    }

    for (const String &E : current->files) {

        list_files.emplace_back(E);
    }

    return OK;
}

String DirAccessPack::get_next() {

    if (m_dir_offset<list_dirs.size()) {
        cdir = true;
        return list_dirs[m_dir_offset++];
    }

    if (m_file_offset<list_files.size()) {
        cdir = false;
        return list_files[m_file_offset++];
    }

    return String();
}
bool DirAccessPack::current_is_dir() const {

    return cdir;
}
bool DirAccessPack::current_is_hidden() const {

    return false;
}
void DirAccessPack::list_dir_end() {
    m_dir_offset = 0;
    m_file_offset = 0;

    list_dirs.set_capacity(0); // clear & free memory
    list_files.set_capacity(0); // clear & free memory
}

int DirAccessPack::get_drive_count() {

    return 0;
}
String DirAccessPack::get_drive(int p_drive) {

    return String();
}

Error DirAccessPack::change_dir(se_string_view p_dir) {

    String nd = PathUtils::from_native_path(p_dir);
    bool absolute = false;
    if (StringUtils::begins_with(nd,"res://")) {
        nd = StringUtils::replace_first(nd,"res://", "");
        absolute = true;
    }

    nd = PathUtils::simplify_path(nd);

    if (nd.empty())
        nd = ".";

    if (StringUtils::begins_with(nd,"/")) {
        nd = StringUtils::replace_first(nd,"/", "");
        absolute = true;
    }

    PODVector<se_string_view> paths = StringUtils::split(nd,'/');

    PackedData::PackedDir *pd;

    if (absolute)
        pd = PackedData::get_singleton()->root;
    else
        pd = current;

    for (int i = 0; i < paths.size(); i++) {

        String p(paths[i]);
        if (p == ".")
            continue;
        if (p == "..") {
            if (pd->parent) {
                pd = pd->parent;
            }
        } else if (pd->subdirs.contains(p)) {

            pd = pd->subdirs[p];

        } else {

            return ERR_INVALID_PARAMETER;
        }
    }

    current = pd;

    return OK;
}

String DirAccessPack::get_current_dir() {

    PackedData::PackedDir *pd = current;
    String p = current->name;

    while (pd->parent) {
        pd = pd->parent;
        p = PathUtils::plus_file(pd->name,p);
    }

    return "res://" + p;
}

bool DirAccessPack::file_exists(se_string_view p_file) {
    p_file = fix_path(p_file);

    return current->files.contains_as(p_file);
}

bool DirAccessPack::dir_exists(se_string_view p_dir) {
    p_dir = fix_path(p_dir);

    return current->subdirs.contains_as(p_dir);
}

Error DirAccessPack::make_dir(se_string_view p_dir) {

    return ERR_UNAVAILABLE;
}

Error DirAccessPack::rename(se_string_view p_from, se_string_view p_to) {

    return ERR_UNAVAILABLE;
}
Error DirAccessPack::remove(se_string_view p_name) {

    return ERR_UNAVAILABLE;
}

size_t DirAccessPack::get_space_left() {

    return 0;
}

String DirAccessPack::get_filesystem_type() const {
    return "PCK";
}

DirAccessPack::DirAccessPack() {

    current = PackedData::get_singleton()->root;
    cdir = false;
}

DirAccessPack::~DirAccessPack() = default;
