/*************************************************************************/
/*  dir_access_windows.cpp                                               */
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

#if defined(WINDOWS_ENABLED)

#include "dir_access_windows.h"

#include "core/os/memory.h"

#include "core/ustring.h"

#include <cstdio>
#include <windows.h>
#include <QStorageInfo>

/*

[03:57] <reduz> yessopie, so i don't havemak to rely on unicows
[03:58] <yessopie> reduz- yeah, all of the functions fail, and then you can call GetLastError () which will return 120
[03:58] <drumstick> CategoryApl, hehe, what? :)
[03:59] <CategoryApl> didn't Verona lead to some trouble
[03:59] <yessopie> 120 = ERROR_CALL_NOT_IMPLEMENTED
[03:59] <yessopie> (you can use that constant if you include winerr.h)
[03:59] <CategoryApl> well answer with winning a compo

[04:02] <yessopie> if ( SetCurrentDirectoryW ( L"." ) == FALSE && GetLastError () == ERROR_CALL_NOT_IMPLEMENTED ) { use
ANSI }
*/

struct DirAccessWindowsPrivate {

    HANDLE h; // handle for findfirstfile
    WIN32_FIND_DATA f;
    WIN32_FIND_DATAW fu; // unicode version
};

// CreateFolderAsync

Error DirAccessWindows::list_dir_begin() {

    _cisdir = false;
    _cishidden = false;

    list_dir_end();
    p->h = FindFirstFileExW((current_dir + "\\*").m_str.toStdWString().c_str(), FindExInfoStandard, &p->fu,
            FindExSearchNameMatch, nullptr, 0);

    return (p->h == INVALID_HANDLE_VALUE) ? ERR_CANT_OPEN : OK;
}

String DirAccessWindows::get_next() {

    if (p->h == INVALID_HANDLE_VALUE) return "";

    _cisdir = (p->fu.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    _cishidden = (p->fu.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN);
    ;
    String name = StringUtils::from_wchar(p->fu.cFileName);

    if (FindNextFileW(p->h, &p->fu) == 0) {

        FindClose(p->h);
        p->h = INVALID_HANDLE_VALUE;
    }

    return name;
}

bool DirAccessWindows::current_is_dir() const {

    return _cisdir;
}

bool DirAccessWindows::current_is_hidden() const {

    return _cishidden;
}

void DirAccessWindows::list_dir_end() {

    if (p->h != INVALID_HANDLE_VALUE) {

        FindClose(p->h);
        p->h = INVALID_HANDLE_VALUE;
    }
}
int DirAccessWindows::get_drive_count() {

    return drive_count;
}
String DirAccessWindows::get_drive(int p_drive) {

    if (p_drive < 0 || p_drive >= drive_count) return "";

    return String(drives[p_drive]) + ":";
}

Error DirAccessWindows::change_dir(String p_dir) {

    GLOBAL_LOCK_FUNCTION
    bool worked = true;
    p_dir = fix_path(p_dir);
    QString actual_wd = QDir::currentPath();
    // try_dir is the directory we are trying to change into
    String try_dir;
    if (PathUtils::is_rel_path(p_dir) || p_dir==".") {
        String next_dir = PathUtils::plus_file(actual_wd, p_dir);
        next_dir = PathUtils::simplify_path(next_dir);
        if (next_dir.empty())
            next_dir = actual_wd+"/.";
        try_dir = next_dir;
    }
    else {
        try_dir = p_dir;
    }
    QFileInfo my_dir(try_dir.m_str);
    if(!my_dir.isDir() || !my_dir.isReadable()) {
        return ERR_INVALID_PARAMETER;
    }

    String base = _get_root_path();
    // If base was set, and new path is not under base, revert the path.
    if (!base.empty() && !StringUtils::begins_with(try_dir,base)) {
        try_dir = current_dir; //revert
        worked = false;
    }
    QDir::setCurrent(actual_wd);
    current_dir = try_dir;
    return worked ? OK : ERR_INVALID_PARAMETER;
}

Error DirAccessWindows::make_dir(String p_dir) {

    GLOBAL_LOCK_FUNCTION

    p_dir = fix_path(p_dir);
    if (PathUtils::is_rel_path(p_dir)) p_dir = PathUtils::plus_file(current_dir,p_dir);

    QDir dir(p_dir.m_str);
    if (!dir.exists()) {
        return dir.mkdir(".") ? OK : ERR_CANT_CREATE;
    }
    return ERR_ALREADY_EXISTS;
}

String DirAccessWindows::get_current_dir() {

    String base = _get_root_path();
    if (base.empty()) return current_dir;

    String bd = StringUtils::replace_first(current_dir,base, "");
    if (StringUtils::begins_with(bd,"/")) return _get_root_string() + StringUtils::substr(bd,1, bd.length());
    return _get_root_string() + bd;
}

bool DirAccessWindows::file_exists(String p_file) {

    GLOBAL_LOCK_FUNCTION

    if (!PathUtils::is_abs_path(p_file)) p_file = PathUtils::plus_file(get_current_dir(),p_file);

    p_file = fix_path(p_file);

    // StringUtils::replace(p_file,"/","\\");
    QFileInfo fi(p_file.m_str);
    return fi.exists() && fi.isFile();
}

bool DirAccessWindows::dir_exists(String p_dir) {

    GLOBAL_LOCK_FUNCTION

    if (PathUtils::is_rel_path(p_dir)) p_dir = PathUtils::plus_file(get_current_dir(),p_dir);

    p_dir = fix_path(p_dir);

    QFileInfo fi(p_dir.m_str);
    return fi.exists() && fi.isDir();
}

Error DirAccessWindows::rename(String p_path, String p_new_path) {

    if (PathUtils::is_rel_path(p_path)) p_path = PathUtils::plus_file(get_current_dir(),p_path);

    p_path = fix_path(p_path);

    if (PathUtils::is_rel_path(p_new_path)) p_new_path = PathUtils::plus_file(get_current_dir(),p_new_path);

    p_new_path = fix_path(p_new_path);
    return QFile::rename(p_path.m_str, p_new_path.m_str) ? OK : FAILED;
}

Error DirAccessWindows::remove(String p_path) {

    if (PathUtils::is_rel_path(p_path)) p_path = PathUtils::plus_file(get_current_dir(),p_path);

    p_path = fix_path(p_path);

    printf("erasing %s\n", qPrintable(p_path.m_str));

    QFileInfo fi(p_path.m_str);
    if (!fi.exists())
        return FAILED;
    if (fi.isDir()) {
        return QDir().rmdir(p_path.m_str) ? OK : FAILED;
    }
    return QFile::remove(p_path.m_str) ? OK : FAILED;
}
/*

FileType DirAccessWindows::get_file_type(const String& p_file) const {


    wchar_t real_current_dir_name[2048];
    GetCurrentDirectoryW(2048,real_current_dir_name);
    String prev_dir=real_current_dir_name;

    bool worked SetCurrentDirectoryW(current_dir.c_str());

    DWORD attr;
    if (worked) {

        WIN32_FILE_ATTRIBUTE_DATA    fileInfo;
        attr = GetFileAttributesExW(p_file.c_str(), GetFileExInfoStandard, &fileInfo);

    }

    SetCurrentDirectoryW(prev_dir.c_str());

    if (!worked)
        return FILE_TYPE_NONE;


    return (attr&FILE_ATTRIBUTE_DIRECTORY)?FILE_TYPE_
}
*/
size_t DirAccessWindows::get_space_left() {

    uint64_t bytes = 0;
    if (!GetDiskFreeSpaceEx(nullptr, (PULARGE_INTEGER)&bytes, nullptr, nullptr)) return 0;

    // this is either 0 or a value in bytes.
    return (size_t)bytes;
}

String DirAccessWindows::get_filesystem_type() const {
    String path = fix_path(const_cast<DirAccessWindows *>(this)->get_current_dir());
    int unit_end = StringUtils::find(path,":");
    ERR_FAIL_COND_V(unit_end == -1, String())
    String unit = StringUtils::substr(path,0, unit_end + 1) + "\\";
    QStorageInfo info(path.m_str);
    return StringUtils::from_utf8(info.fileSystemType());
}

DirAccessWindows::DirAccessWindows() {

    p = memnew(DirAccessWindowsPrivate);
    p->h = INVALID_HANDLE_VALUE;
    current_dir = ".";

    drive_count = 0;

#ifdef UWP_ENABLED
    Windows::Storage::StorageFolder ^ install_folder = Windows::ApplicationModel::Package::Current->InstalledLocation;
    change_dir(install_folder->Path->Data());

#else

    DWORD mask = GetLogicalDrives();

    for (int i = 0; i < MAX_DRIVES; i++) {

        if (mask & (1 << i)) { // DRIVE EXISTS

            drives[drive_count] = 'A' + i;
            drive_count++;
        }
    }

    DirAccessWindows::change_dir(".");
#endif
}

DirAccessWindows::~DirAccessWindows() {

    memdelete(p);
}

#endif // windows DirAccess support
