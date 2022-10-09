/*************************************************************************/
/*  find_in_files.h                                                      */
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

#include "core/hash_map.h"
#include "core/map.h"
#include "core/set.h"
#include "scene/gui/dialogs.h"

// Performs the actual search
class FindInFiles : public Node {
    GDCLASS(FindInFiles,Node)

public:
    static const char *SIGNAL_RESULT_FOUND;
    static const char *SIGNAL_FINISHED;

    FindInFiles();

    void set_search_text(String p_pattern);
    void set_whole_words(bool p_whole_word);
    void set_match_case(bool p_match_case);
    void set_folder(StringView folder);
    void set_filter(const Set<String> &exts);

    const String & get_search_text() const { return _pattern; }

    bool is_whole_words() const { return _whole_words; }
    bool is_match_case() const { return _match_case; }

    void start();
    void stop();

    bool is_searching() const { return _searching; }
    float get_progress() const;

protected:
    void _notification(int p_notification);

    static void _bind_methods();

private:
    void _process();
    void _iterate();
    void _scan_dir(StringView path, PoolVector<String> &out_folders);
    void _scan_file(StringView fpath);

    // Config
    String _pattern;
    Set<String> _extension_filter;
    String _root_dir;
    bool _whole_words;
    bool _match_case;

    // State
    bool _searching;
    String _current_dir;
    Vector<PoolVector<String>> _folders_stack;
    Vector<String> _files_to_scan;
    int _initial_files_count;
};

class LineEdit;
class CheckBox;
class FileDialog;
class HBoxContainer;

// Prompts search parameters
class FindInFilesDialog : public AcceptDialog {
    GDCLASS(FindInFilesDialog,AcceptDialog)

public:
    enum FindInFilesMode { SEARCH_MODE, REPLACE_MODE };
    static const char *SIGNAL_FIND_REQUESTED;
    static const char *SIGNAL_REPLACE_REQUESTED;

    FindInFilesDialog();

    void set_search_text(StringView text);
    void set_replace_text(StringView text);

    void set_find_in_files_mode(FindInFilesMode p_mode);

    String get_search_text() const;
    String get_replace_text() const;
    bool is_match_case() const;
    bool is_whole_words() const;
    String get_folder() const;
    Set<String> get_filter() const;

protected:
    static void _bind_methods();

    void _notification(int p_what);
    void custom_action(StringView p_action) override;

private:
    void _on_folder_button_pressed();
    void _on_folder_selected(StringView path);
    void _on_search_text_modified(StringView text);
    void _on_search_text_entered(StringView text);
    void _on_replace_text_entered(StringView text);

    LineEdit *_search_text_line_edit;
    LineEdit *_folder_line_edit;
    CheckBox *_match_case_checkbox;
    CheckBox *_whole_words_checkbox;
    Label *_replace_label;
    LineEdit *_replace_text_line_edit;
    Button *_find_button;
    Button *_replace_button;
    FileDialog *_folder_dialog;
    HBoxContainer *_filters_container;
    HashMap<String, bool> _filters_preferences;
    FindInFilesMode _mode;
};

class Button;
class Tree;
class TreeItem;
class ProgressBar;

// Display search results
class FindInFilesPanel : public Control {
    GDCLASS(FindInFilesPanel,Control)

public:
    static const char *SIGNAL_RESULT_SELECTED;
    static const char *SIGNAL_FILES_MODIFIED;

    FindInFilesPanel();

    FindInFiles *get_finder() const { return _finder; }

    void set_with_replace(bool with_replace);
    void set_replace_text(StringView text);

    void start_search();
    void stop_search();

protected:
    static void _bind_methods();

    void _notification(int p_what);

private:
    void _on_result_found(StringView fpath, int line_number, int begin, int end, const String &text);
    void _on_finished();
    void _on_refresh_button_clicked();
    void _on_cancel_button_clicked();
    void _on_result_selected();
    void _on_item_edited();
    void _on_replace_text_changed(StringView text);
    void _on_replace_all_clicked();

    struct Result {
        int line_number;
        int begin;
        int end;
        float draw_begin;
        float draw_width;
    };

    void apply_replaces_in_file(StringView fpath, const Vector<Result> &locations, StringView new_text);
    void update_replace_buttons();
    String get_replace_text();

    void draw_result_text(Object *item_obj, Rect2 rect);

    void set_progress_visible(bool visible);
    void clear();

    FindInFiles *_finder;
    Label *_search_text_label;
    Tree *_results_display;
    Label *_status_label;
    Button *_refresh_button;
    Button *_cancel_button;
    ProgressBar *_progress_bar;
    Map<String, TreeItem *> _file_items;
    HashMap<TreeItem *, Result> _result_items;
    bool _with_replace;

    HBoxContainer *_replace_container;
    LineEdit *_replace_line_edit;
    Button *_replace_all_button;
};

void register_find_in_files_classes();
