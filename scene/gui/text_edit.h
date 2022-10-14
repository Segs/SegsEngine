/*************************************************************************/
/*  text_edit.h                                                          */
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

#include "core/list.h"
#include "core/script_language.h"
#include "scene/gui/control.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/scroll_bar.h"
#include "scene/main/timer.h"
#include "core/hash_set.h"

#include <utility>

class SyntaxHighlighter;

struct TextColorRegionInfo {

    int region=0;
    bool end=false;
};
class GODOT_EXPORT TextEdit : public Control {

    GDCLASS(TextEdit,Control)

public:
    struct HighlighterInfo {
        Color color;
    };

    struct ColorRegionData {
        Color color;
        uint32_t begin_key_len=0;
        uint32_t end_key_len=0;
        bool eq=true;
        bool line_only;
    };
    friend void start_stop_idle_detection(TextEdit *,bool);
private:
    void *m_priv=nullptr;

    bool readonly;
    bool syntax_coloring;
    bool indent_using_spaces;
    int indent_size;

    Timer *caret_blink_timer;
    bool caret_blink_enabled;
    bool draw_caret=true;
    bool window_has_focus;
    bool block_caret;
    bool right_click_moves_caret;

    bool first_draw;
    bool draw_tabs=false;
    bool draw_spaces=false;
    bool override_selected_font_color=false;
    bool line_numbers;
    bool line_numbers_zero_padded;
    bool line_length_guidelines;
    int line_length_guideline_soft_col;
    int line_length_guideline_hard_col;
    bool draw_bookmark_gutter;
    bool draw_breakpoint_gutter;
    int breakpoint_gutter_width;
    bool draw_fold_gutter;
    int fold_gutter_width;
    bool draw_info_gutter;
    int info_gutter_width;
    bool draw_minimap;
    int minimap_width;
    Point2 minimap_char_size;
    int minimap_line_spacing;

    bool highlight_all_occurrences;
    bool scroll_past_end_of_file_enabled;
    bool auto_brace_completion_enabled;
    bool brace_matching_enabled;
    bool highlight_current_line;
    bool auto_indent;

    bool insert_mode;
    bool select_identifiers_enabled;

    bool smooth_scroll_enabled;
    bool scrolling;
    bool dragging_minimap;
    bool can_drag_minimap;
    bool minimap_clicked;
    double minimap_scroll_ratio;
    double minimap_scroll_click_pos;
    float target_v_scroll;
    float v_scroll_speed;


    Vector2 last_dblclk_pos;
    uint64_t last_dblclk;

    Timer *idle_detect;
    HScrollBar *h_scroll;
    VScrollBar *v_scroll;
    bool updating_scrolls;

    GameEntity tooltip_obj_id;
    StringName tooltip_func;
    Variant tooltip_ud;

    bool callhint_below;
    Vector2 callhint_offset;

    int search_result_line;
    int search_result_col;

    bool selecting_enabled;
    bool deselect_on_focus_loss_enabled;
    bool popup_show = false;
    bool context_menu_enabled;
    bool shortcut_keys_enabled;
    bool middle_mouse_paste_enabled=true;
    bool drag_action = false;
    bool drag_caret_force_displayed = false;

    int executing_line;
public:
    void _generate_context_menu();
    int get_visible_rows() const;
    int get_total_visible_rows() const;

    int _get_minimap_visible_rows() const;
    void update_cursor_wrap_offset();
    void _update_wrap_at();
    bool is_line_wrapped(int line) const;
    int get_line_wrap_count(int line) const;
    Vector<UIString> get_wrap_rows_text(int p_line) const;
    int get_cursor_wrap_index() const;
    int get_line_wrap_index_at_col(int p_line, int p_column) const;
    int get_char_count();

    double get_scroll_pos_for_line(int p_line, int p_wrap_index = 0) const;
    void set_line_as_first_visible(int p_line, int p_wrap_index = 0);
    void set_line_as_center_visible(int p_line, int p_wrap_index = 0);
    void set_line_as_last_visible(int p_line, int p_wrap_index = 0);
    int get_first_visible_line() const;
    int get_last_full_visible_line() const;
    int get_last_full_visible_line_wrap_index() const;
    double get_visible_rows_offset() const;
    double get_v_scroll_offset() const;

    int get_char_pos_for_line(int p_px, int p_line, int p_wrap_index = 0) const;
    int get_column_x_offset_for_line(int p_char, int p_line) const;
    int get_char_pos_for(int p_px, const UIString& p_str) const;
    int get_column_x_offset(int p_char, const UIString& p_str) const;

    void adjust_viewport_to_cursor();
    void _scroll_moved(double);
    void _update_scrollbars();
    void _v_scroll_input();
    void _click_selection_held();

    void _update_minimap_hover();
    void _update_minimap_click();
    void _update_minimap_drag();
    void _scroll_up(real_t p_delta);
    void _scroll_down(real_t p_delta);

    void _pre_shift_selection();
    void _post_shift_selection();

    void _scroll_lines_up();
    void _scroll_lines_down();

    //void mouse_motion(const Point& p_pos, const Point& p_rel, int p_button_mask);
    Size2 get_minimum_size() const override;
    int _get_control_height() const;

    int get_row_height() const;

    void _reset_caret_blink_timer();
    void _toggle_draw_caret();

    void _update_caches();
    void _cursor_changed_emit();
    void _text_changed_emit();


    PopupMenu *menu;

    void _clear();
    void _confirm_completion();
    void _update_completion_candidates();

    int _calculate_spaces_till_next_left_indent(int column);
    int _calculate_spaces_till_next_right_indent(int column);

protected:
    const String & get_tooltip(const Point2 &p_pos) const override;

    void _gui_input(const Ref<InputEvent> &p_gui_input);
    void _notification(int p_what);
    void _push_current_op(); // slot
    static void _bind_methods();
public:
    // made public in _bind_methods as "search"
    PoolVector<int> _search_bind(StringView _key, uint32_t p_search_flags, int p_from_line, int p_from_column) const;

public:
    SyntaxHighlighter *_get_syntax_highlighting();
    void _set_syntax_highlighting(SyntaxHighlighter *p_syntax_highlighter);

    int _is_line_in_region(int p_line);
    ColorRegionData _get_color_region(int p_region) const;
    Map<int, TextColorRegionInfo> _get_line_color_region_info(int p_line) const;

    enum MenuItems {
        MENU_CUT,
        MENU_COPY,
        MENU_PASTE,
        MENU_CLEAR,
        MENU_SELECT_ALL,
        MENU_UNDO,
        MENU_REDO,
        MENU_MAX

    };

    enum SearchFlags {

        SEARCH_MATCH_CASE = 1,
        SEARCH_WHOLE_WORDS = 2,
        SEARCH_BACKWARDS = 4
    };
    enum SearchResult {
        SEARCH_RESULT_COLUMN,
        SEARCH_RESULT_LINE,
    };

    CursorShape get_cursor_shape(const Point2 &p_pos = Point2i()) const override;
    Variant get_drag_data(const Point2 &p_point) override;
    bool can_drop_data(const Point2 &p_point, const Variant &p_data) const override;
    void drop_data(const Point2 &p_point, const Variant &p_data) override;

    void _get_mouse_pos(const Point2i &p_mouse, int &r_row, int &r_col) const;
    void _get_minimap_mouse_row(const Point2i &p_mouse, int &r_row) const;

    //void delete_char();
    //void delete_line();

    void begin_complex_operation();
    void end_complex_operation();

    bool is_insert_text_operation();

    void set_text(StringView p_text);
    void set_text_ui(const UIString& p_text);
    void insert_text_at_cursor(StringView p_text);
    void insert_text_at_cursor_ui(const UIString &p_text);
    void insert_at(const UIString &p_text, int at);
    int get_line_count() const;
    int get_line_width(int p_line, int p_wrap_index = -1) const;
    int get_line_height() const;
    void set_line_as_marked(int p_line, bool p_marked);
    void set_line_as_bookmark(int p_line, bool p_bookmark);
    bool is_line_set_as_bookmark(int p_line) const;
    void get_bookmarks(Vector<int> *p_bookmarks) const;
    Array get_bookmarks_array() const;
    void set_line_as_breakpoint(int p_line, bool p_breakpoint);
    bool is_line_set_as_breakpoint(int p_line) const;
    void get_breakpoints(Vector<int> *p_breakpoints) const;
    Array get_breakpoints_array() const;
    void remove_breakpoints();
    void set_executing_line(int p_line);
    void clear_executing_line();
    void set_line_as_safe(int p_line, bool p_safe);
    bool is_line_set_as_safe(int p_line) const;

    void set_line_info_icon(int p_line, const Ref<Texture>& p_icon, StringName p_info = StringName());
    void clear_info_icons();

    void set_line_as_hidden(int p_line, bool p_hidden);
    bool is_line_hidden(int p_line) const;
    void fold_all_lines();
    void unhide_all_lines();
    int num_lines_from(int p_line_from, int visible_amount) const;
    int num_lines_from_rows(int p_line_from, int p_wrap_index_from, int visible_amount, int &wrap_index) const;
    int get_last_unhidden_line() const;

    bool can_fold(int p_line) const;
    bool is_folded(int p_line) const;
    Vector<int> get_folded_lines() const;
    void fold_line(int p_line);
    void unfold_line(int p_line);
    void toggle_fold_line(int p_line);

    String get_text_utf8() const;
    String get_text();
    //String get_line(int line) const;
    String get_line(int line) const;
    void set_line(int line, StringView new_text);
    void backspace_at_cursor();

    void indent_left();
    void indent_right();
    int get_indent_level(int p_line) const;
    bool is_line_comment(int p_line) const;

    inline void set_scroll_pass_end_of_file(bool p_enabled) {
        scroll_past_end_of_file_enabled = p_enabled;
        update();
    }
    inline void set_auto_brace_completion(bool p_enabled) {
        auto_brace_completion_enabled = p_enabled;
    }
    inline void set_brace_matching(bool p_enabled) {
        brace_matching_enabled = p_enabled;
        update();
    }
    inline void set_callhint_settings(bool below, Vector2 offset) {
        callhint_below = below;
        callhint_offset = offset;
    }
    void set_auto_indent(bool p_auto_indent);

    void center_viewport_to_cursor();

    void cursor_set_column(int p_col, bool p_adjust_viewport = true);
    void cursor_set_line(int p_row, bool p_adjust_viewport = true, bool p_can_be_hidden = true, int p_wrap_index = 0);

    int cursor_get_column() const;
    int cursor_get_line() const;
    Vector2i _get_cursor_pixel_pos();

    bool cursor_get_blink_enabled() const;
    void cursor_set_blink_enabled(const bool p_enabled);

    float cursor_get_blink_speed() const;
    void cursor_set_blink_speed(const float p_speed);

    void cursor_set_block_mode(const bool p_enable);
    bool cursor_is_block_mode() const;

    void set_right_click_moves_caret(bool p_enable);
    bool is_right_click_moving_caret() const;

    void set_readonly(bool p_readonly);
    bool is_readonly() const;


    void set_wrap_enabled(bool p_wrap_enabled);
    bool is_wrap_enabled() const;

    void clear();

    void set_syntax_coloring(bool p_enabled);
    bool is_syntax_coloring_enabled() const;

    void cut();
    void copy();
    void paste();
    void select_all();
    void select(int p_from_line, int p_from_column, int p_to_line, int p_to_column);
    void deselect();
    void swap_lines(int line1, int line2);

    void set_search_text(const UIString &p_search_text);
    void set_search_flags(uint32_t p_flags);
    void set_current_search_result(int line, int col);

    void set_highlight_all_occurrences(const bool p_enabled);
    bool is_highlight_all_occurrences_enabled() const;
    bool is_selection_active() const;
    int get_selection_from_line() const;
    int get_selection_from_column() const;
    int get_selection_to_line() const;
    int get_selection_to_column() const;
    String get_selection_text() const;
    bool is_mouse_over_selection(bool p_edges = true) const;

    String get_word_under_cursor() const;
    String get_word_at_pos(const Vector2 &p_pos) const;

    /* Line and character position. */
    Point2 get_pos_at_line_column(int p_line, int p_column) const;
    Rect2 get_rect_at_line_column(int p_line, int p_column) const;
    Point2 get_line_column_at_pos(const Point2 &p_pos) const;
    bool search(const UIString &p_key, uint32_t p_search_flags, int p_from_line, int p_from_column, int &r_line, int &r_column) const;

    bool has_undo() const;
    bool has_redo() const;
    void undo();
    void redo();
    void clear_undo_history();

    void set_indent_using_spaces(const bool p_use_spaces);
    bool is_indent_using_spaces() const;
    void set_indent_size(const int p_size);
    int get_indent_size();
    void set_draw_tabs(bool p_draw);
    bool is_drawing_tabs() const;
    void set_draw_spaces(bool p_draw);
    bool is_drawing_spaces() const;
    void set_override_selected_font_color(bool p_override_selected_font_color);
    bool is_overriding_selected_font_color() const;

    void set_insert_mode(bool p_enabled);
    bool is_insert_mode() const;

    void add_keyword_color(StringView p_keyword, const Color &p_color);
    bool has_keyword_color_uistr(const UIString& p_keyword) const;
    bool has_keyword_color(StringView p_keyword) const;
    Color get_keyword_color_uistr(const UIString& p_keyword) const;
    Color get_keyword_color(StringView p_keyword) const;

    void add_color_region(StringView p_begin_key = {}, StringView p_end_key = {}, const Color &p_color = Color(), bool p_line_only = false);
    void clear_colors();

    void add_member_keyword(StringView p_keyword, const Color &p_color);
    bool has_member_color(const UIString& p_member) const;
    Color get_member_color(const UIString& p_member) const;
    void clear_member_keywords();

    double get_v_scroll() const;
    void set_v_scroll(double p_scroll);

    int get_h_scroll() const;
    void set_h_scroll(int p_scroll);

    void set_smooth_scroll_enabled(bool p_enable);
    bool is_smooth_scroll_enabled() const;

    void set_v_scroll_speed(float p_speed);
    float get_v_scroll_speed() const;

    uint32_t get_version() const;
    uint32_t get_saved_version() const;
    void tag_saved_version();

    void menu_option(int p_option);

    void set_show_line_numbers(bool p_show);
    bool is_show_line_numbers_enabled() const;

    void set_highlight_current_line(bool p_enabled);
    bool is_highlight_current_line_enabled() const;

    void set_line_numbers_zero_padded(bool p_zero_padded);

    void set_show_line_length_guidelines(bool p_show);
    void set_line_length_guideline_soft_column(int p_column);
    void set_line_length_guideline_hard_column(int p_column);

    void set_bookmark_gutter_enabled(bool p_draw);
    bool is_bookmark_gutter_enabled() const;

    void set_breakpoint_gutter_enabled(bool p_draw);
    bool is_breakpoint_gutter_enabled() const;

    void set_breakpoint_gutter_width(int p_gutter_width);
    int get_breakpoint_gutter_width() const;

    void set_draw_fold_gutter(bool p_draw);
    bool is_drawing_fold_gutter() const;

    void set_fold_gutter_width(int p_gutter_width);
    int get_fold_gutter_width() const;

    void set_draw_info_gutter(bool p_draw);
    bool is_drawing_info_gutter() const;

    void set_info_gutter_width(int p_gutter_width);
    int get_info_gutter_width() const;
    int get_total_gutter_width() const;
    void set_draw_minimap(bool p_draw);
    bool is_drawing_minimap() const;

    void set_minimap_width(int p_minimap_width);
    int get_minimap_width() const;

    void set_hiding_enabled(bool p_enabled);
    bool is_hiding_enabled() const;

    void set_tooltip_request_func(Object *p_obj, const StringName &p_function, const Variant &p_udata);

    void set_completion(bool p_enabled, const Vector<UIString> &p_prefixes);
    void code_complete(const Vector<ScriptCodeCompletionOption> &p_strings, bool p_forced = false);
    void set_code_hint(const String &p_hint);
    void query_code_comple();

    void set_select_identifiers_on_hover(bool p_enable);
    bool is_selecting_identifiers_on_hover_enabled() const;

    void set_context_menu_enabled(bool p_enable);
    bool is_context_menu_enabled();

    void set_selecting_enabled(bool p_enabled);
    bool is_selecting_enabled() const;
    void set_deselect_on_focus_loss_enabled(const bool p_enabled);
    bool is_deselect_on_focus_loss_enabled() const;

    void set_shortcut_keys_enabled(bool p_enabled);
    bool is_shortcut_keys_enabled() const;
    void set_middle_mouse_paste_enabled(bool p_enabled);
    bool is_middle_mouse_paste_enabled() const;

    PopupMenu *get_menu() const;

    UIString get_text_for_completion();
    String get_text_for_completion_utf8() const;
    String get_text_for_lookup_completion();

    bool is_text_field() const override;
    TextEdit();
    ~TextEdit() override;
};


class SyntaxHighlighter {
protected:
    TextEdit *text_editor;

public:
    virtual ~SyntaxHighlighter() = default;
    virtual void _update_cache() = 0;
    virtual Map<int, TextEdit::HighlighterInfo> _get_line_syntax_highlighting(int p_line) = 0;

    virtual String get_name() const = 0;
    virtual Vector<String> get_supported_languages() = 0;

    void set_text_editor(TextEdit *p_text_editor);
    TextEdit *get_text_editor();
};
