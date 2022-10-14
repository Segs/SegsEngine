/*************************************************************************/
/*  editor_help.h                                                        */
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

#include "editor/code_editor.h"
#include "editor/doc/doc_builder.h"
#include "editor/editor_plugin.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel_container.h"

#include "scene/gui/split_container.h"
#include "scene/gui/tab_container.h"

#include "scene/main/timer.h"

class RichTextLabel;

namespace DocContents {
    struct ArgumentDoc;
    struct MethodDoc;
}

class FindBarPrivate;
class FindBar : public HBoxContainer {
    GDCLASS(FindBar,HBoxContainer)

    FindBarPrivate *m_private;
    int results_count;

    void _hide_bar();

    void _search_text_changed(StringView p_text);
    void _search_text_entered(StringView p_text);

    void _update_results_count();
    void _update_matches_label();


protected:
    void _notification(int p_what);
    void _unhandled_input(const Ref<InputEvent> &p_event);

    bool _search(bool p_search_previous = false);

    static void _bind_methods();

public:
    void set_rich_text_label(RichTextLabel *p_rich_text_label);

    void popup_search();

    bool search_prev();
    bool search_next();

    FindBar();
    ~FindBar() override;
};
class EditorHelpPrivate;
class EditorHelp : public VBoxContainer {

    GDCLASS(EditorHelp,VBoxContainer)

    enum Page {

        PAGE_CLASS_LIST,
        PAGE_CLASS_DESC,
        PAGE_CLASS_PREV,
        PAGE_CLASS_NEXT,
        PAGE_SEARCH,
        CLASS_SEARCH,

    };

    bool select_locked;

    String prev_search;

    StringName edited_class;
    EditorHelpPrivate *m_private;
    int description_line;

    RichTextLabel *class_desc;
    HSplitContainer *h_split;
    static DocData *doc;

    ConfirmationDialog *search_dialog;
    LineEdit *search;
    FindBar *find_bar;

    String base_path;

    Color title_color;
    Color text_color;
    Color headline_color;
    Color base_type_color;
    Color type_color;
    Color comment_color;
    Color symbol_color;
    Color value_color;
    Color qualifier_color;

    void _init_colors();
    void _help_callback(StringView p_topic);

    void _add_text(StringView p_bbcode);
    void _add_text(const UIString &p_bbcode);
    bool scroll_locked;

    //void _button_pressed(int p_idx);
    void _add_type(StringView p_type, StringView p_enum = {});
    void _add_method(const DocContents::MethodDoc &p_method, bool p_overview = true);
    void _add_bulletpoint();

    void _class_list_select(StringView p_select);
    void _class_desc_select(StringView p_select);
    void _class_desc_input(const Ref<InputEvent> &p_input);
    void _class_desc_resized();

    Error _goto_desc(StringView p_class, int p_vscr = -1);
    //void _update_history_buttons();
    void _update_doc();

    void _request_help(StringView p_string);
    void _search(bool p_search_previous=false);

    void _unhandled_key_input(const Ref<InputEvent> &p_ev);

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    static void generate_doc();
    static DocData *get_doc_data() { return doc; }

    void go_to_help(StringView p_help);
    void go_to_class(StringView p_class, int p_scroll = 0);

    Vector<Pair<String, int> > get_sections();
    void scroll_to_section(int p_section_index);

    void popup_search();
    void search_again(bool p_search_previous=false);

    const char * get_class() const override;

    void set_focused();

    int get_scroll() const;
    void set_scroll(int p_scroll);

    EditorHelp();
    ~EditorHelp() override;
};

class EditorHelpBit : public PanelContainer {

    GDCLASS(EditorHelpBit,PanelContainer)

    RichTextLabel *rich_text;
    void _go_to_help(const StringName &p_what);
    void _meta_clicked(StringView p_select);

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    RichTextLabel *get_rich_text() { return rich_text; }
    void set_text(StringView p_text);
    void set_text_ui(const UIString &p_text);
    EditorHelpBit();
};
