/*************************************************************************/
/*  dependency_editor.h                                                  */
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

#include "editor_file_dialog.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/tree.h"

class EditorFileDialog;
class EditorFileSystemDirectory;
class EditorNode;

class DependencyEditor : public AcceptDialog {
    GDCLASS(DependencyEditor,AcceptDialog)

    Tree *tree;
    Button *fixdeps;

    EditorFileDialog *search;

    se_string replacing;
    se_string editing;
    ListPOD<se_string> missing;

    void _fix_and_find(EditorFileSystemDirectory *efsd, Map<se_string_view, Map<se_string, se_string> > &candidates);

    void _searched(se_string_view p_path);
    void _load_pressed(Object *p_item, int p_cell, int p_button);
    void _fix_all();
    void _update_list();

    void _update_file();

protected:
    static void _bind_methods();

public:
    void edit(se_string_view p_path);
    DependencyEditor();
};

class DependencyEditorOwners : public AcceptDialog {
    GDCLASS(DependencyEditorOwners,AcceptDialog)

    ItemList *owners;
    PopupMenu *file_options;
    EditorNode *editor;
    se_string editing;

    void _fill_owners(EditorFileSystemDirectory *efsd);

    static void _bind_methods();
    void _list_rmb_select(int p_item, const Vector2 &p_pos);
    void _select_file(int p_idx);
    void _file_option(int p_option);

private:
    enum FileMenu {
        FILE_OPEN
    };

public:
    void show(se_string_view p_path);
    DependencyEditorOwners(EditorNode *p_editor);
};

class DependencyRemoveDialog : public ConfirmationDialog {
    GDCLASS(DependencyRemoveDialog,ConfirmationDialog)

    Label *text;
    Tree *owners;

    Map<se_string, se_string> all_remove_files;
    Vector<se_string> dirs_to_delete;
    Vector<se_string> files_to_delete;

    struct RemovedDependency {
        se_string file;
        se_string file_type;
        se_string dependency;
        se_string dependency_folder;

        bool operator<(const RemovedDependency &p_other) const {
            if (dependency_folder.empty() != p_other.dependency_folder.empty()) {
                return p_other.dependency_folder.empty();
            } else {
                return dependency < p_other.dependency;
            }
        }
    };

    void _find_files_in_removed_folder(EditorFileSystemDirectory *efsd, se_string_view p_folder);
    void _find_all_removed_dependencies(EditorFileSystemDirectory *efsd, Vector<RemovedDependency> &p_removed);
    void _build_removed_dependency_tree(const Vector<RemovedDependency> &p_removed);

    void ok_pressed() override;

    static void _bind_methods();

public:
    void show(const Vector<se_string> &p_folders, const Vector<se_string> &p_files);
    DependencyRemoveDialog();
};

class DependencyErrorDialog : public ConfirmationDialog {
    GDCLASS(DependencyErrorDialog,ConfirmationDialog)

public:
    enum Mode {
        MODE_SCENE,
        MODE_RESOURCE,
    };

private:
    se_string for_file;
    Mode mode;
    Button *fdep;
    Label *text;
    Tree *files;
    void ok_pressed() override;
    void custom_action(se_string_view) override;

public:
    void show(Mode p_mode, se_string_view p_for_file, const Vector<se_string> &report);
    DependencyErrorDialog();
};

class OrphanResourcesDialog : public ConfirmationDialog {
    GDCLASS(OrphanResourcesDialog,ConfirmationDialog)

    DependencyEditor *dep_edit;
    Tree *files;
    ConfirmationDialog *delete_confirm;
    void ok_pressed() override;

    bool _fill_owners(EditorFileSystemDirectory *efsd, HashMap<se_string, int> &refs, TreeItem *p_parent);

    List<se_string> paths;
    void _find_to_delete(TreeItem *p_item, List<se_string> &paths);
    void _delete_confirm();
    void _button_pressed(Object *p_item, int p_column, int p_id);

    void refresh();
    static void _bind_methods();

public:
    void show();
    OrphanResourcesDialog();
};
