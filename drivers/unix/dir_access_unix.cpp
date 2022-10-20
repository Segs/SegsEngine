/*************************************************************************/
/*  dir_access_unix.cpp                                                  */
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

#include "dir_access_unix.h"
#include "core/fixed_string.h"

#if defined(UNIX_ENABLED) || defined(LIBC_FILEIO_ENABLED)

#include "core/list.h"
#include "core/os/memory.h"
#include "core/print_string.h"
#include "core/string.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"
#include "core/vector.h"

#include "EASTL/sort.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef ANDROID_ENABLED
#include <sys/statvfs.h>
#endif

#ifdef HAVE_MNTENT
#include <mntent.h>
#endif

DirAccess *DirAccessUnix::create_fs() {

    return memnew(DirAccessUnix);
}

Error DirAccessUnix::list_dir_begin() {

    list_dir_end(); //close any previous dir opening!

    //char real_current_dir_name[2048]; //is this enough?!
    //getcwd(real_current_dir_name,2048);
    //chdir(StringUtils::to_utf8(current_path).data());
    dir_stream = opendir(current_dir.data());
    //chdir(real_current_dir_name);
    if (!dir_stream)
        return ERR_CANT_OPEN; //error!

    return OK;
}

bool DirAccessUnix::file_exists(StringView _file) {

    String p_file(_file);
    GLOBAL_LOCK_FUNCTION

    if (PathUtils::is_rel_path(p_file))
        p_file = PathUtils::plus_file(current_dir,p_file);

    p_file = fix_path(p_file);

    struct stat flags;
    bool success = (stat(p_file.data(), &flags) == 0);

    if (success && S_ISDIR(flags.st_mode)) {
        success = false;
    }

    return success;
}

bool DirAccessUnix::dir_exists(StringView _dir) {

    String p_dir(_dir);
    GLOBAL_LOCK_FUNCTION

    if (PathUtils::is_rel_path(p_dir))
        p_dir = PathUtils::plus_file(get_current_dir(),p_dir);

    p_dir = fix_path(p_dir);

    struct stat flags;
    bool success = (stat(p_dir.data(), &flags) == 0);

    return (success && S_ISDIR(flags.st_mode));
}

uint64_t DirAccessUnix::get_modified_time(StringView _file) {
    String p_file(_file);
    if (PathUtils::is_rel_path(p_file))
        p_file = PathUtils::plus_file(current_dir,p_file);

    p_file = fix_path(p_file);

    struct stat flags;
    bool success = (stat(p_file.data(), &flags) == 0);

    if (success) {
        return flags.st_mtime;
    } else {

        ERR_FAIL_V(0);
    }
    return 0;
}

bool DirAccessUnix::is_hidden(StringView p_name) {
    return p_name != StringView(".") && p_name != StringView("..") && StringUtils::begins_with(p_name,".");
}

bool DirAccessUnix::has_next() const {
    if (!dir_stream) {
        return false;
    }
    auto offset=telldir(dir_stream);
    bool has_entry = readdir(dir_stream)!=nullptr;
    seekdir(dir_stream,offset); //reset scan position
    return has_entry;
}

String DirAccessUnix::get_next() {

    if (!dir_stream)
        return String();

    dirent *entry = readdir(dir_stream);

    if (entry == nullptr) {
        list_dir_end();
        return String();
    }

    String fname = fix_unicode_name(entry->d_name);

    // Look at d_type to determine if the entry is a directory, unless
    // its type is unknown (the file system does not support it) or if
    // the type is a link, in that case we want to resolve the link to
    // known if it points to a directory. stat() will resolve the link
    // for us.
    if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK) {
        String f = PathUtils::plus_file(current_dir,fname);

        struct stat flags;
        if (stat(f.data(), &flags) == 0) {
            _cisdir = S_ISDIR(flags.st_mode);
        } else {
            _cisdir = false;
        }
    } else {
        _cisdir = (entry->d_type == DT_DIR);
    }

    _cishidden = is_hidden(fname);

    return fname;
}

bool DirAccessUnix::current_is_dir() const {

    return _cisdir;
}

bool DirAccessUnix::current_is_hidden() const {

    return _cishidden;
}

void DirAccessUnix::list_dir_end() {

    if (dir_stream)
        closedir(dir_stream);
    dir_stream = nullptr;
    _cisdir = false;
}

#if defined(HAVE_MNTENT) && defined(X11_ENABLED)
static bool _filter_drive(struct mntent *mnt) {
    // Ignore devices that don't point to /dev
    if (strncmp(mnt->mnt_fsname, "/dev", 4) != 0) {
        return false;
    }

    // Accept devices mounted at common locations
    if (strncmp(mnt->mnt_dir, "/media", 6) == 0 ||
            strncmp(mnt->mnt_dir, "/mnt", 4) == 0 ||
            strncmp(mnt->mnt_dir, "/home", 5) == 0 ||
            strncmp(mnt->mnt_dir, "/run/media", 10) == 0) {
        return true;
    }

    // Ignore everything else
    return false;
}
#endif

static void _get_drives(Vector<String> *vec) {

    vec->push_back("/");
#if defined(HAVE_MNTENT) && defined(X11_ENABLED)
    // Check /etc/mtab for the list of mounted partitions
    FILE *mtab = setmntent("/etc/mtab", "r");
    if (mtab) {
        struct mntent mnt;
        char strings[4096];

        while (getmntent_r(mtab, &mnt, strings, sizeof(strings))) {
            if (mnt.mnt_dir != nullptr && _filter_drive(&mnt)) {
                // Avoid duplicates
                if (!list->find(mnt.mnt_dir)) {
                    list->push_back(mnt.mnt_dir);
                }
            }
        }

        endmntent(mtab);
    }
#endif

    // Add $HOME
    const char *home = getenv("HOME");
    if (home) {
        // Only add if it's not a duplicate
        if (!vec->contains(home)) {
            vec->push_back(home);
        }

        // Check $HOME/.config/gtk-3.0/bookmarks
        char path[1024];
        snprintf(path, 1024, "%s/.config/gtk-3.0/bookmarks", home);
        FILE *fd = fopen(path, "r");
        if (fd) {
            char string[1024];
            while (fgets(string, 1024, fd)) {
                StringView string_sv(string);
                // Parse only file:// links
                if (string_sv.starts_with("file://")) {
                    // Strip any unwanted edges on the strings and push_back if it's not a duplicate
                    String fpath = StringUtils::percent_decode(StringUtils::split_spaces(StringUtils::strip_edges(string_sv.substr(7)))[0]);
                    if (!vec->contains(fpath)) {
                        vec->push_back(fpath);
                    }
                }
            }

            fclose(fd);
        }
    }
    eastl::sort(vec->begin(),vec->end());
}

int DirAccessUnix::get_drive_count() {

    Vector<String> list;
    _get_drives(&list);

    return list.size();
}

String DirAccessUnix::get_drive(int p_drive) {

    Vector<String> list;
    _get_drives(&list);

    ERR_FAIL_INDEX_V(p_drive, list.size(), String());

    return list[p_drive];
}

bool DirAccessUnix::drives_are_shortcuts() {

    return true;
}

int DirAccessUnix::get_current_drive() {
    int drive = 0;
    int max_length = -1;
    const String path = StringUtils::to_lower(get_current_dir());
    for (int i = 0; i < get_drive_count(); i++) {
        const String d = StringUtils::to_lower(get_drive(i));
        if (max_length < d.length() && StringUtils::begins_with(path,d)) {
            max_length = d.length();
            drive = i;
        }
    }
    return drive;
}
Error DirAccessUnix::make_dir(StringView _dir) {
    String p_dir(_dir);
    GLOBAL_LOCK_FUNCTION

    if (PathUtils::is_rel_path(p_dir))
        p_dir = PathUtils::plus_file(get_current_dir(),p_dir);

    p_dir = fix_path(p_dir);

    bool success = (mkdir(p_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0);
    int err = errno;

    if (success) {
        return OK;
    }

    if (err == EEXIST) {
        return ERR_ALREADY_EXISTS;
    }

    return ERR_CANT_CREATE;
}

Error DirAccessUnix::change_dir(StringView _dir) {

    GLOBAL_LOCK_FUNCTION

    String p_dir = fix_path(_dir);

    // prev_dir is the directory we are changing out of
    UIString prev_dir;
    char real_current_dir_name[2048];
    ERR_FAIL_COND_V(getcwd(real_current_dir_name, 2048) == nullptr, ERR_BUG);
    if (StringUtils::parse_utf8(prev_dir,real_current_dir_name))
        prev_dir = real_current_dir_name; //no utf8, maybe latin?

    // try_dir is the directory we are trying to change into
    String try_dir;
    if (PathUtils::is_rel_path(p_dir)) {
        String next_dir = PathUtils::plus_file(current_dir,p_dir);
        next_dir = PathUtils::simplify_path(next_dir);
        try_dir = next_dir;
    } else {
        try_dir = p_dir;
    }

    bool worked = (chdir(try_dir.data()) == 0); // we can only give this utf8
    if (!worked) {
        return ERR_INVALID_PARAMETER;
    }

    String base = _get_root_path();
    if (!base.empty() && !StringUtils::begins_with(try_dir,base)) {
        ERR_FAIL_COND_V(getcwd(real_current_dir_name, 2048) == nullptr, ERR_BUG);
        String new_dir = real_current_dir_name;

        if (!StringUtils::begins_with(new_dir,base)) {
            try_dir = current_dir; //revert
        }
    }

    // the directory exists, so set current_dir to try_dir
    current_dir = try_dir;
    ERR_FAIL_COND_V(chdir(StringUtils::to_utf8(prev_dir).data()) != 0, ERR_BUG);
    return OK;
}

String DirAccessUnix::get_current_dir() {

    String base = _get_root_path();
    if (!base.empty()) {

        String bd = StringUtils::replace_first(current_dir,base, String());
        if (StringUtils::begins_with(bd,"/"))
            return _get_root_string() + StringUtils::substr(bd,1, bd.length());
        else
            return _get_root_string() + bd;
    }
    return current_dir;
}

Error DirAccessUnix::rename(StringView _path, StringView _new_path) {
    String p_path(_path);
    String p_new_path(_new_path);

    if (PathUtils::is_rel_path(p_path))
        p_path = PathUtils::plus_file(get_current_dir(),p_path);

    p_path = fix_path(p_path);

    if (PathUtils::is_rel_path(p_new_path))
        p_new_path = PathUtils::plus_file(get_current_dir(),p_new_path);

    p_new_path = fix_path(p_new_path);

    return ::rename(p_path.data(), p_new_path.data()) == 0 ? OK : FAILED;
}

Error DirAccessUnix::remove(StringView _path) {

    String p_path(_path);
    if (PathUtils::is_rel_path(p_path))
        p_path = PathUtils::plus_file(get_current_dir(),p_path);

    p_path = fix_path(p_path);

    struct stat flags;
    if ((stat(p_path.data(), &flags) != 0))
        return FAILED;

    if (S_ISDIR(flags.st_mode))
        return ::rmdir(p_path.data()) == 0 ? OK : FAILED;
    else
        return ::unlink(p_path.data()) == 0 ? OK : FAILED;
}


bool DirAccessUnix::is_link(StringView p_file_view) {
    using namespace PathUtils;
    TmpString<1024> p_file(p_file_view);
    if (is_rel_path(p_file))
        p_file.assign(plus_file(get_current_dir(),p_file).c_str());

    p_file = fix_path(p_file).c_str();

    struct stat flags;
    if ((lstat(p_file.data(), &flags) != 0))
        return FAILED;

    return S_ISLNK(flags.st_mode);
}

String DirAccessUnix::read_link(StringView p_file_view) {
    using namespace PathUtils;
    String p_file(p_file_view);

    if (is_rel_path(p_file))
        p_file = plus_file(get_current_dir(),p_file);

    p_file = fix_path(p_file);

    char buf[256];
    memset(buf, 0, 256);
    ssize_t len = readlink(p_file.data(), buf, sizeof(buf));
    if (len > 0) {
        return String(buf,len);
    }
    return {};
}

Error DirAccessUnix::create_link(StringView p_source, StringView p_target_view) {
    using namespace PathUtils;
    String p_target(p_target_view);
    if (is_rel_path(p_target))
        p_target = plus_file(get_current_dir(),p_target);

    p_source = fix_path(p_source);
    p_target = fix_path(p_target);

    if (symlink(String(p_source).c_str(), p_target.data()) == 0) {
        return OK;
    } else {
        return FAILED;
    }
}

uint64_t DirAccessUnix::get_space_left() {

#ifndef NO_STATVFS
    struct statvfs vfs;
    if (statvfs(current_dir.data(), &vfs) != 0) {

        return 0;
    }

    return (uint64_t)vfs.f_bavail * (uint64_t)vfs.f_frsize;
#else
    // FIXME: Implement this.
    return 0;
#endif
}

String DirAccessUnix::get_filesystem_type() const {
    return String(); //TODO this should be implemented
}

DirAccessUnix::DirAccessUnix() {

    dir_stream = nullptr;
    _cisdir = false;

    /* determine drive count */

    // set current directory to an absolute path of the current directory
    char real_current_dir_name[2048];
    auto res=getcwd(real_current_dir_name, 2048);
    ERR_FAIL_COND(res == nullptr);
    current_dir = real_current_dir_name;

    change_dir(current_dir);
}

DirAccessUnix::~DirAccessUnix() {

    DirAccessUnix::list_dir_end();
}

#endif //posix_enabled
