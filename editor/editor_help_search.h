/*************************************************************************/
/*  editor_help_search.h                                                 */
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

#include "core/ordered_hash_map.h"
#include "editor/code_editor.h"
#include "editor/editor_help.h"
#include "editor/editor_plugin.h"
#include "scene/gui/option_button.h"
#include "scene/gui/tree.h"

class EditorHelpSearch : public ConfirmationDialog {
    GDCLASS(EditorHelpSearch,ConfirmationDialog)

    class Runner;

    enum SearchFlags {
        SEARCH_CLASSES = 1 << 0,
        SEARCH_METHODS = 1 << 1,
        SEARCH_SIGNALS = 1 << 2,
        SEARCH_CONSTANTS = 1 << 3,
        SEARCH_PROPERTIES = 1 << 4,
        SEARCH_THEME_ITEMS = 1 << 5,
        SEARCH_FLAG_ALL = (SEARCH_CLASSES | SEARCH_METHODS | SEARCH_SIGNALS | SEARCH_CONSTANTS | SEARCH_PROPERTIES | SEARCH_THEME_ITEMS),
        SEARCH_CASE_SENSITIVE = 1 << 29,
        SEARCH_SHOW_HIERARCHY = 1 << 30
    };

    LineEdit *search_box;
    ToolButton *case_sensitive_button;
    ToolButton *hierarchy_button;
    OptionButton *filter_combo;
    Tree *results_tree;
    Ref<Runner> search;
    bool old_search;
    se_string old_term;

    void _update_icons();
    void _update_results();

    void _search_box_gui_input(const Ref<InputEvent> &p_event);
    void _search_box_text_changed(se_string_view p_text);
    void _filter_combo_item_selected(int p_option);
    void _confirmed();

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    void popup_dialog();
    void popup_dialog(se_string_view p_term);

    EditorHelpSearch();
};
