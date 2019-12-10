/*************************************************************************/
/*  dir_access.cpp                                                       */
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

#include "dir_access.h"

#include "core/os/file_access.h"
#include "core/os/memory.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/string_utils.h"

se_string DirAccess::_get_root_path() const {

    switch (_access_type) {

        case ACCESS_RESOURCES: return ProjectSettings::get_singleton()->get_resource_path();
        case ACCESS_USERDATA: return OS::get_singleton()->get_user_data_dir();
        default: return se_string();
    }
}
se_string DirAccess::_get_root_string() const {

    switch (_access_type) {

        case ACCESS_RESOURCES: return "res://";
        case ACCESS_USERDATA: return "user://";
        default: return se_string();
    }
}

int DirAccess::get_current_drive() {

    se_string path = StringUtils::to_lower(get_current_dir());
    for (int i = 0; i < get_drive_count(); i++) {
        se_string d = StringUtils::to_lower(get_drive(i));
        if (StringUtils::begins_with(path,d))
            return i;
    }

    return 0;
}

Error DirAccess::change_dir_utf8(se_string_view p_dir)
{
    return change_dir(p_dir);
}

Error DirAccess::make_dir_utf8(se_string_view p_dir)
{
    return make_dir(p_dir);

}

static Error _erase_recursive(DirAccess *da) {

    List<se_string> dirs;
    List<se_string> files;

    da->list_dir_begin();
    se_string n(da->get_next());
    while (!n.empty()) {

        if (n != "." && n != "..") {

            if (da->current_is_dir())
                dirs.push_back(n);
            else
                files.push_back(n);
        }

        n = da->get_next();
    }

    da->list_dir_end();

    for (List<se_string>::Element *E = dirs.front(); E; E = E->next()) {

        Error err = da->change_dir(E->deref());
        if (err == OK) {

            err = _erase_recursive(da);
            if (err) {
                da->change_dir("..");
                return err;
            }
            err = da->change_dir("..");
            if (err) {
                return err;
            }
            err = da->remove(PathUtils::plus_file(da->get_current_dir(),E->deref()));
            if (err) {
                return err;
            }
        } else {
            return err;
        }
    }

    for (List<se_string>::Element *E = files.front(); E; E = E->next()) {

        Error err = da->remove(PathUtils::plus_file(da->get_current_dir(),E->deref()));
        if (err) {
            return err;
        }
    }

    return OK;
}

Error DirAccess::erase_contents_recursive() {

    return _erase_recursive(this);
}

Error DirAccess::make_dir_recursive(se_string_view p_dir) {

    if (p_dir.length() < 1) {
        return OK;
    }

    se_string full_dir;

    if (PathUtils::is_rel_path(p_dir)) {
        //append current
        full_dir = PathUtils::plus_file(get_current_dir(),p_dir);

    } else {
        full_dir = p_dir;
    }

    full_dir = PathUtils::from_native_path(full_dir);

    //int slices = StringUtils::get_slice_count(full_dir"/");

    se_string base;

    if (StringUtils::begins_with(full_dir,"res://"))
        base = "res://";
    else if (StringUtils::begins_with(full_dir,"user://"))
        base = "user://";
    else if (StringUtils::begins_with(full_dir,"/"))
        base = "/";
    else if (StringUtils::contains(full_dir,":/")) {
        base = StringUtils::substr(full_dir,0, StringUtils::find(full_dir,":/") + 2);
    } else {
        ERR_FAIL_V(ERR_INVALID_PARAMETER)
    }

    full_dir = PathUtils::simplify_path(StringUtils::replace_first(full_dir,base, ""));

    FixedVector<se_string_view,16,true> subdirs;
    se_string::split_ref(subdirs,full_dir,'/');

    se_string curpath = base;
    for (int i = 0; i < subdirs.size(); i++) {

        curpath = PathUtils::plus_file(curpath,subdirs[i]);
        Error err = make_dir(curpath);
        if (err != OK && err != ERR_ALREADY_EXISTS) {

            ERR_FAIL_V(err)
        }
    }

    return OK;
}

se_string DirAccess::fix_path(se_string_view p_path) const {

    switch (_access_type) {

        case ACCESS_RESOURCES: {

            if (ProjectSettings::get_singleton()) {
                if (StringUtils::begins_with(p_path,"res://")) {

                    se_string resource_path = ProjectSettings::get_singleton()->get_resource_path();
                    if (!resource_path.empty()) {

                        return StringUtils::replace_first(p_path,"res:/", resource_path);
                    }
                    return StringUtils::replace_first(p_path,"res://", "");
                }
            }

        } break;
        case ACCESS_USERDATA: {

            if (StringUtils::begins_with(p_path,"user://")) {

                se_string data_dir = OS::get_singleton()->get_user_data_dir();
                if (!data_dir.empty()) {

                    return StringUtils::replace_first(p_path,"user:/", data_dir);
                }
                return StringUtils::replace_first(p_path,"user://", "");
            }

        } break;
        case ACCESS_FILESYSTEM: {

            return se_string(p_path);
        }
        case ACCESS_MAX: break; // Can't happen, but silences warning
    }

    return se_string(p_path);
}

DirAccess::CreateFunc DirAccess::create_func[ACCESS_MAX] = { nullptr, nullptr, nullptr };

DirAccess *DirAccess::create_for_path(se_string_view p_path) {

    DirAccess *da = nullptr;
    if (StringUtils::begins_with(p_path,"res://")) {

        da = create(ACCESS_RESOURCES);
    } else if (StringUtils::begins_with(p_path,"user://")) {

        da = create(ACCESS_USERDATA);
    } else {

        da = create(ACCESS_FILESYSTEM);
    }

    return da;
}

DirAccess *DirAccess::open(se_string_view p_path, Error *r_error) {

    DirAccess *da = create_for_path(p_path);

    ERR_FAIL_COND_V_MSG(!da, nullptr, "Cannot create DirAccess for path '" + se_string(p_path) + "'.")
    Error err = da->change_dir(p_path);
    if (r_error)
        *r_error = err;
    if (err != OK) {
        memdelete(da);
        return nullptr;
    }

    return da;
}

DirAccess *DirAccess::create(AccessType p_access) {

    DirAccess *da = create_func[p_access] ? create_func[p_access]() : nullptr;
    if (da) {
        da->_access_type = p_access;
    }

    return da;
};

se_string DirAccess::get_full_path(se_string_view p_path, AccessType p_access) {

    DirAccess *d = DirAccess::create(p_access);
    if (!d)
        return se_string(p_path);

    d->change_dir(p_path);
    se_string full = d->get_current_dir();
    memdelete(d);
    return full;
}

Error DirAccess::copy(se_string_view p_from, se_string_view p_to, int p_chmod_flags) {

    //printf("copy %s -> %s\n",p_from.ascii().get_data(),p_to.ascii().get_data());
    Error err;
    FileAccess *fsrc = FileAccess::open(p_from, FileAccess::READ, &err);

    if (err) {
        ERR_PRINT("Failed to open " + se_string(p_from))
        return err;
    }

    FileAccess *fdst = FileAccess::open(p_to, FileAccess::WRITE, &err);
    if (err) {

        fsrc->close();
        memdelete(fsrc);
        ERR_PRINT("Failed to open " + se_string(p_to))
        return err;
    }

    fsrc->seek_end(0);
    int size = fsrc->get_position();
    fsrc->seek(0);
    err = OK;
    while (size--) {

        if (fsrc->get_error() != OK) {
            err = fsrc->get_error();
            break;
        }
        if (fdst->get_error() != OK) {
            err = fdst->get_error();
            break;
        }

        fdst->store_8(fsrc->get_8());
    }

    if (err == OK && p_chmod_flags != -1) {
        fdst->close();
        err = FileAccess::set_unix_permissions(p_to, p_chmod_flags);
        // If running on a platform with no chmod support (i.e., Windows), don't fail
        if (err == ERR_UNAVAILABLE)
            err = OK;
    }

    memdelete(fsrc);
    memdelete(fdst);

    return err;
}

void DirAccess::remove_file_or_error(se_string_view  p_path) {
    DirAccess *da = create(ACCESS_FILESYSTEM);
    if (da->file_exists(p_path)) {
        if (da->remove(p_path) != OK) {
            ERR_FAIL_MSG("Cannot remove file or directory: " + se_string(p_path))
        }
    }
    memdelete(da);
}

// Changes dir for the current scope, returning back to the original dir
// when scope exits
class DirChanger {
    DirAccess *da;
    se_string original_dir;

public:
    DirChanger(DirAccess *p_da, se_string_view p_dir) :
        da(p_da),
        original_dir(p_da->get_current_dir()) {
        p_da->change_dir(p_dir);
    }

    ~DirChanger() {
        da->change_dir(original_dir);
    }
};

Error DirAccess::_copy_dir(DirAccess *p_target_da, se_string_view  p_to, int p_chmod_flags) {
    List<se_string_view> dirs;

    se_string curdir = get_current_dir();
    list_dir_begin();
    se_string n = get_next();
    while (!n.empty()) {

        if (n != "." && n != "..") {

            if (current_is_dir())
                dirs.push_back(n);
            else {
                se_string_view rel_path = n;
                if (!PathUtils::is_rel_path(n)) {
                    list_dir_end();
                    return ERR_BUG;
                }
                Error err = copy(PathUtils::plus_file(get_current_dir(),n), se_string(p_to) + rel_path, p_chmod_flags);
                if (err) {
                    list_dir_end();
                    return err;
                }
            }
        }

        n = get_next();
    }

    list_dir_end();

    for (List<se_string_view>::Element *E = dirs.front(); E; E = E->next()) {
        se_string_view rel_path = E->deref();
        se_string target_dir = se_string(p_to) + rel_path;
        if (!p_target_da->dir_exists(target_dir)) {
            Error err = p_target_da->make_dir(target_dir);
            ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot create directory '" + target_dir + "'.")
        }

        Error err = change_dir(E->deref());
        ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot change current directory to '" + se_string(rel_path) + "'.")
        err = _copy_dir(p_target_da, se_string(p_to) + rel_path + "/", p_chmod_flags);
        if (err) {
            change_dir("..");
            ERR_FAIL_V_MSG(err, "Failed to copy recursively.")
        }
        err = change_dir("..");
        ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to go back.")
    }

    return OK;
}

Error DirAccess::copy_dir(se_string_view  p_from, se_string_view p_to, int p_chmod_flags) {
    ERR_FAIL_COND_V_MSG(!dir_exists(p_from), ERR_FILE_NOT_FOUND, "Source directory doesn't exist.")

    DirAccess *target_da = DirAccess::create_for_path(p_to);
    ERR_FAIL_COND_V_MSG(!target_da, ERR_CANT_CREATE, "Cannot create DirAccess for path '" + se_string(p_to) + "'.")

    if (!target_da->dir_exists(p_to)) {
        Error err = target_da->make_dir_recursive(p_to);
        if (err) {
            memdelete(target_da);
        }
        ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot create directory '" + se_string(p_to) + "'.")
    }

    DirChanger dir_changer(this, p_from);
    Error err;
    se_string p_to_fix(p_to);
    if (!p_to.ends_with('/')) {
        p_to_fix.push_back('/');
    }
    err = _copy_dir(target_da, p_to_fix, p_chmod_flags);

    memdelete(target_da);

    return err;
}

bool DirAccess::exists(se_string_view  p_dir) {

    DirAccess *da = DirAccess::create_for_path(p_dir);
    bool valid = da->change_dir(p_dir) == OK;
    memdelete(da);
    return valid;
}
