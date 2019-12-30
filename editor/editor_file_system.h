/*************************************************************************/
/*  editor_file_system.h                                                 */
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

#include "core/os/dir_access.h"
#include "core/os/thread.h"
#include "core/os/thread_safe.h"
#include "core/list.h"
#include "core/set.h"
#include "core/hash_map.h"

#include "core/se_string.h"
#include "core/translation_helpers.h"
#include "scene/main/node.h"
class FileAccess;

struct EditorProgressBG;
class EditorFileSystemDirectory : public Object {

    GDCLASS(EditorFileSystemDirectory,Object)

    se_string name;
    uint64_t modified_time;
    bool verified; //used for checking changes

    EditorFileSystemDirectory *parent;
    PODVector<EditorFileSystemDirectory *> subdirs;

    struct FileInfo {
        se_string file;
        StringName type;
        uint64_t modified_time;
        uint64_t import_modified_time;
        bool import_valid;
        se_string import_group_file;
        Vector<se_string> deps;
        bool verified; //used for checking changes
        StringName script_class_name;
        StringName script_class_extends;
        se_string script_class_icon_path;
    };

    struct FileInfoSort {
        bool operator()(const FileInfo *p_a, const FileInfo *p_b) const {
            return p_a->file < p_b->file;
        }
    };

    void sort_files();

    PODVector<FileInfo *> files;

    static void _bind_methods();

    friend class EditorFileSystem;

public:
    const se_string &get_name();
    se_string get_path() const;

    int get_subdir_count() const;
    EditorFileSystemDirectory *get_subdir(int p_idx);
    int get_file_count() const;
    se_string get_file(int p_idx) const;
    se_string get_file_path(int p_idx) const;
    se_string get_named_file_path(se_string_view file) const;
    StringName get_file_type(int p_idx) const;
    const Vector<se_string> &get_file_deps(int p_idx) const;
    bool get_file_import_is_valid(int p_idx) const;
    StringName get_file_script_class_name(int p_idx) const; //used for scripts
    StringName get_file_script_class_extends(int p_idx) const; //used for scripts
    const se_string &get_file_script_class_icon_path(int p_idx) const; //used for scripts

    EditorFileSystemDirectory *get_parent();

    int find_file_index(se_string_view p_file) const;
    int find_dir_index(se_string_view p_dir) const;

    EditorFileSystemDirectory();
    ~EditorFileSystemDirectory() override;
};

class EditorFileSystem : public Node {

    GDCLASS(EditorFileSystem,Node)

    _THREAD_SAFE_CLASS_

    struct ItemAction {

        enum Action {
            ACTION_NONE,
            ACTION_DIR_ADD,
            ACTION_DIR_REMOVE,
            ACTION_FILE_ADD,
            ACTION_FILE_REMOVE,
            ACTION_FILE_TEST_REIMPORT,
            ACTION_FILE_RELOAD
        };

        Action action;
        EditorFileSystemDirectory *dir;
        se_string file;
        EditorFileSystemDirectory *new_dir;
        EditorFileSystemDirectory::FileInfo *new_file;

        ItemAction() {
            action = ACTION_NONE;
            dir = nullptr;
            new_dir = nullptr;
            new_file = nullptr;
        }
    };

    bool use_threads;
    Thread *thread;
    static void _thread_func(void *_userdata);

    EditorFileSystemDirectory *new_filesystem;

    bool abort_scan;
    bool scanning;
    bool importing;
    bool first_scan;
    float scan_total;
    se_string filesystem_settings_version_for_import;
    bool revalidate_import_files;

    void _scan_filesystem();

    Set<se_string> late_added_files; //keep track of files that were added, these will be re-scanned
    Set<se_string> late_update_files;

    void _save_late_updated_files();

    EditorFileSystemDirectory *filesystem;

    static EditorFileSystem *singleton;

    /* Used for reading the filesystem cache file */
    struct FileCache {

        se_string type;
        uint64_t modification_time;
        uint64_t import_modification_time;
        Vector<se_string> deps;
        bool import_valid;
        se_string import_group_file;
        StringName script_class_name;
        StringName script_class_extends;
        se_string script_class_icon_path;
    };

    HashMap<se_string, FileCache> file_cache;

    struct ScanProgress {

        float low;
        float hi;
        mutable EditorProgressBG *progress;
        void update(int p_current, int p_total) const;
        ScanProgress get_sub(int p_current, int p_total) const;
    };

    void _save_filesystem_cache();
    void _save_filesystem_cache(EditorFileSystemDirectory *p_dir, FileAccess *p_file);

    bool _find_file(se_string_view p_file, EditorFileSystemDirectory **r_d, int &r_file_pos) const;

    void _scan_fs_changes(EditorFileSystemDirectory *p_dir, const ScanProgress &p_progress);

    void _delete_internal_files(se_string_view p_file);

    Set<se_string> valid_extensions;
    Set<se_string> import_extensions;

    void _scan_new_dir(EditorFileSystemDirectory *p_dir, DirAccess *da, const ScanProgress &p_progress);

    Thread *thread_sources;
    bool scanning_changes;
    bool scanning_changes_done;

    static void _thread_func_sources(void *_userdata);

    ListPOD<String> sources_changed;
    ListPOD<ItemAction> scan_actions;

    bool _update_scan_actions();

    void _update_extensions();

    void _reimport_file(const se_string &p_file);
    Error _reimport_group(se_string_view p_group_file, const Vector<se_string> &p_files);

    bool _test_for_reimport(se_string_view p_path, bool p_only_imported_files);

    bool reimport_on_missing_imported_files;

    Vector<se_string> _get_dependencies(se_string_view p_path);

    struct ImportFile {
        se_string path;
        int order;
        bool operator<(const ImportFile &p_if) const {
            return order < p_if.order;
        }
    };

    void _scan_script_classes(EditorFileSystemDirectory *p_dir);
    volatile bool update_script_classes_queued;
    void _queue_update_script_classes();

    StringName _get_global_script_class(se_string_view p_type, se_string_view p_path, StringName *r_extends, se_string *r_icon_path) const;

    static Error _resource_import(se_string_view p_path);

    bool using_fat32_or_exfat; // Workaround for projects in FAT32 or exFAT filesystem (pendrives, most of the time)

    void _find_group_files(EditorFileSystemDirectory *efd, DefMap<se_string, Vector<se_string> > &group_files, Set<se_string> &groups_to_reimport);

    void _move_group_files(EditorFileSystemDirectory *efd, se_string_view p_group_file, se_string_view p_new_location);

    Set<se_string> group_file_cache;

protected:
    void _notification(int p_what);
    static void _bind_methods();

    void _scan_from_cache();
    void _scan_mark_updates();

public:
    static EditorFileSystem *get_singleton() { return singleton; }

    EditorFileSystemDirectory *get_filesystem();
    bool is_scanning() const;
    bool is_importing() const { return importing; }
    float get_scanning_progress() const;
    void scan();
    void scan_changes();
    void get_changed_sources(ListPOD<String> *r_changed);
    void update_file(se_string_view p_file);

    EditorFileSystemDirectory *get_filesystem_path(se_string_view p_path);
    se_string get_file_type(se_string_view p_file) const;
    EditorFileSystemDirectory *find_file(se_string_view p_file, int *r_index) const;

    void reimport_files(const Vector<se_string> &p_files);

    void update_script_classes();

    bool is_group_file(se_string_view p_path) const;
    void move_group_file(se_string_view p_path, se_string_view p_new_path);

    EditorFileSystem();
    ~EditorFileSystem() override;
};
