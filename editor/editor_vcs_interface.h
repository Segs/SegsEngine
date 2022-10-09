/*************************************************************************/
/*  editor_vcs_interface.h                                               */
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

#include "core/object.h"
#include "core/string.h"
#include "scene/gui/panel_container.h"

class GODOT_EXPORT EditorVCSInterface : public Object {

    GDCLASS(EditorVCSInterface, Object)

    bool is_initialized;
public:
    enum ChangeType {
        CHANGE_TYPE_NEW = 0,
        CHANGE_TYPE_MODIFIED = 1,
        CHANGE_TYPE_RENAMED = 2,
        CHANGE_TYPE_DELETED = 3,
        CHANGE_TYPE_TYPECHANGE = 4,
        CHANGE_TYPE_UNMERGED = 5
    };

    enum TreeArea {
        TREE_AREA_COMMIT = 0,
        TREE_AREA_STAGED = 1,
        TREE_AREA_UNSTAGED = 2
    };

    struct DiffLine {
        int new_line_no;
        int old_line_no;
        String content;
        String status;

        String old_text;
        String new_text;
    };

    struct DiffHunk {
        int new_start;
        int old_start;
        int new_lines;
        int old_lines;
        Vector<DiffLine> diff_lines;
    };

    struct DiffFile {
        String new_file;
        String old_file;
        Vector<DiffHunk> diff_hunks;
    };

    struct Commit {
        String author;
        String msg;
        String id;
        int64_t unix_timestamp;
        int64_t offset_minutes;
    };

    struct StatusFile {
        TreeArea area;
        ChangeType change_type;
        String file_path;
    };

protected:
    static EditorVCSInterface *singleton;

    static void _bind_methods();
    DiffLine _convert_diff_line(Dictionary p_diff_line);
    DiffHunk _convert_diff_hunk(Dictionary p_diff_hunk);
    DiffFile _convert_diff_file(Dictionary p_diff_file);
    Commit _convert_commit(Dictionary p_commit);
    StatusFile _convert_status_file(Dictionary p_status_file);

    // Implemented by addons as end points for the proxy functions
    virtual bool _initialize(StringView p_project_root_path);
    virtual bool _is_vcs_initialized();
    virtual Dictionary _get_modified_files_data();
    virtual void _stage_file(StringView p_file_path);
    virtual void _unstage_file(StringView p_file_path);
    virtual void _commit(StringView p_msg);
    virtual Array _get_file_diff(StringView p_file_path);
    virtual bool _shut_down();
    virtual String _get_project_name();
    virtual String _get_vcs_name();

public:
    static EditorVCSInterface *get_singleton();
    static void set_singleton(EditorVCSInterface *p_singleton);

    bool is_addon_ready();

    // Proxy functions to the editor for use
    bool initialize(StringView p_project_root_path);
    bool is_vcs_initialized();
    Dictionary get_modified_files_data();
    void stage_file(StringView p_file_path);
    void unstage_file(StringView p_file_path);
    void commit(StringView p_msg);
    Array get_file_diff(StringView p_file_path);
    bool shut_down();
    String get_project_name();
    String get_vcs_name();

    EditorVCSInterface();
    ~EditorVCSInterface() override;
};
