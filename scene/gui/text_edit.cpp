/*************************************************************************/
/*  text_edit.cpp                                                        */
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

#include "text_edit.h"

#include "label.h"

#include "core/callable_method_pointer.h"
#include "core/message_queue.h"
#include "core/method_bind.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/script_language.h"
#include "core/string_utils.inl"
#include "core/translation_helpers.h"
#include "core/ustring.h"

#include "scene/main/viewport.h"
#include "scene/resources/font.h"
#include "scene/resources/style_box.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_scale.h"
#endif

#include <utility>

template<>
struct eastl::hash<QStringRef> {
    size_t operator()(QStringRef p) const { return StringUtils::hash(p.constData(), p.length()); }
};

void start_stop_idle_detection(TextEdit *textedit,bool start) {
    if(start) {
        if(textedit->idle_detect->is_inside_tree()) {
            textedit->idle_detect->start();
        }
    }
}


namespace  {
inline bool _is_symbol(CharType c) {

    return is_symbol(c);
}

static bool _te_is_text_char(CharType c) {

    return !is_symbol(c);
}

static bool _is_whitespace(CharType c) {
    return c == '\t' || c == ' ';
}

static bool _is_char(CharType c) {

    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool _is_number(CharType c) {
    return (c >= '0' && c <= '9');
}

static bool _is_hex_symbol(CharType c) {
    return ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

static bool _is_pair_right_symbol(CharType c) {
    return c == '"' || c == '\'' || c == ')' || c == ']' || c == '}';
}

static bool _is_pair_left_symbol(CharType c) {
    return c == '"' || c == '\'' || c == '(' || c == '[' || c == '{';
}

static bool _is_pair_symbol(CharType c) {
    return _is_pair_left_symbol(c) || _is_pair_right_symbol(c);
}

static CharType _get_right_pair_symbol(CharType c) {
    if (c == '"')
        return '"';
    if (c == '\'')
        return '\'';
    if (c == '(')
        return ')';
    if (c == '[')
        return ']';
    if (c == '{')
        return '}';
    return 0;
}

static int _find_first_non_whitespace_column_of_line(const UIString &line) {
    int left = 0;
    while (left < line.length() && _is_whitespace(line[left]))
        left++;
    return left;
}
struct TextColorRegion {

    Color color;
    UIString begin_key;
    UIString end_key;
    bool line_only;
    bool eq;
    TextColorRegion(const UIString &p_begin_key = UIString(), const UIString &p_end_key = UIString(),
            const Color &p_color = Color(), bool p_line_only = false) {
        begin_key = p_begin_key;
        end_key = p_end_key;
        color = p_color;
        line_only = p_line_only || p_end_key.isEmpty();
        eq = begin_key == end_key;
    }
};
class Text {
public:
    struct Line {
        int width_cache : 24;
        bool marked : 1;
        bool breakpoint : 1;
        bool bookmark : 1;
        bool hidden : 1;
        bool safe : 1;
        bool has_info : 1;
        int wrap_amount_cache : 24;
        Map<int, TextColorRegionInfo> region_info;
        Ref<Texture> info_icon;
        StringName info;
        UIString data;

        Line() {
            width_cache = 0;
            marked = false;
            breakpoint = false;
            bookmark = false;
            hidden = false;
            safe = false;
            has_info = false;
            wrap_amount_cache = 0;
        }
    };

private:
    const Vector<TextColorRegion> *color_regions;
    mutable Vector<Line> text;
    Ref<Font> font;
    int indent_size;

    void _update_line_cache(uint32_t p_line) const;

public:
    void set_indent_size(int p_indent_size);
    void set_font(const Ref<Font> &p_font);
    void set_color_regions(const Vector<TextColorRegion> *p_regions) { color_regions = p_regions; }
    int get_line_width(int p_line) const;
    int get_max_width(bool p_exclude_hidden = false) const;
    int get_char_width(CharType c, CharType next_c, int px) const;
    void set_line_wrap_amount(int p_line, int p_wrap_amount) const;
    int get_line_wrap_amount(int p_line) const;
    const Map<int, TextColorRegionInfo> &get_color_region_info(int p_line) const;
    void set(int p_line, const UIString &p_text);
    void set_marked(uint32_t p_line, bool p_marked) { text[p_line].marked = p_marked; }
    bool is_marked(uint32_t p_line) const { return text[p_line].marked; }
    void set_bookmark(uint32_t p_line, bool p_bookmark) { text[p_line].bookmark = p_bookmark; }
    bool is_bookmark(uint32_t p_line) const { return text[p_line].bookmark; }
    void set_breakpoint(uint32_t p_line, bool p_breakpoint) { text[p_line].breakpoint = p_breakpoint; }
    bool is_breakpoint(uint32_t p_line) const { return text[p_line].breakpoint; }
    void set_hidden(uint32_t p_line, bool p_hidden) { text[p_line].hidden = p_hidden; }
    bool is_hidden(uint32_t p_line) const { return text[p_line].hidden; }
    void set_safe(uint32_t p_line, bool p_safe) { text[p_line].safe = p_safe; }
    bool is_safe(uint32_t p_line) const { return text[p_line].safe; }
    void set_info_icon(uint32_t p_line, Ref<Texture> p_icon, StringName p_info) {
        if (p_icon) {
            text[p_line].has_info = false;
            return;
        }
        text[p_line].info_icon = p_icon;
        text[p_line].info = p_info;
        text[p_line].has_info = true;
    }
    bool has_info_icon(uint32_t p_line) const { return text[p_line].has_info; }
    const Ref<Texture> &get_info_icon(uint32_t p_line) const { return text[p_line].info_icon; }
    StringName get_info(uint32_t p_line) const { return text[p_line].info; }
    void insert(int p_at, const UIString &p_text);
    void remove(int p_at);
    size_t size() const { return text.size(); }
    void clear();
    void clear_width_cache();
    void clear_wrap_cache();
    void clear_info_icons() {
        for (uint32_t i = 0; i < text.size(); i++) {
            text[i].has_info = false;
        }
    }
    _FORCE_INLINE_ const UIString &operator[](int p_line) const { return text[p_line].data; }
    Text() { indent_size = 4; }
};
struct PrivateData {
    struct Cache {

        Ref<Texture> tab_icon;
        Ref<Texture> space_icon;
        Ref<Texture> can_fold_icon;
        Ref<Texture> folded_icon;
        Ref<Texture> folded_eol_icon;
        Ref<Texture> executing_icon;
        Ref<StyleBox> style_normal;
        Ref<StyleBox> style_focus;
        Ref<StyleBox> style_readonly;
        Ref<Font> font;
        Color completion_background_color;
        Color completion_selected_color;
        Color completion_existing_color;
        Color completion_font_color;
        Color caret_color;
        Color caret_background_color;
        Color line_number_color;
        Color safe_line_number_color;
        Color font_color;
        Color font_color_selected;
        Color font_color_readonly;
        Color keyword_color;
        Color control_flow_keyword_color;
        Color number_color;
        Color function_color;
        Color member_variable_color;
        Color selection_color;
        Color mark_color;
        Color bookmark_color;
        Color breakpoint_color;
        Color executing_line_color;
        Color code_folding_color;
        Color current_line_color;
        Color line_length_guideline_color;
        Color brace_mismatch_color;
        Color word_highlighted_color;
        Color search_result_color;
        Color search_result_border_color;
        Color symbol_color;
        Color background_color;

        int row_height=0;
        int line_spacing=0;
        int line_number_w=0;
        int breakpoint_gutter_width=0;
        int fold_gutter_width=0;
        int info_gutter_width=0;
        int minimap_width=0;
    };
    struct Cursor {
        int last_fit_x=0;
        int line=0, column=0; ///< cursor
        int x_ofs=0, line_ofs=0, wrap_ofs=0;
    };
    struct Selection {

        enum Mode : uint8_t {

            MODE_NONE=0,
            MODE_SHIFT,
            MODE_POINTER,
            MODE_WORD,
            MODE_LINE
        };

        int selecting_line=0, selecting_column=0;
        int selected_word_beg=0, selected_word_end=0, selected_word_origin=0;

        int from_line=0, from_column=0;
        int to_line=0, to_column=0;

        Mode selecting_mode=MODE_NONE;
        bool selecting_text=false;
        bool active=false;
        bool shiftclick_left=false;

        bool drag_attempt = false;
    };

    struct TextOperation {

        enum Type : uint8_t { TYPE_NONE = 0, TYPE_INSERT, TYPE_REMOVE };

        UIString text;
        int from_line=0, from_column=0;
        int to_line=0, to_column=0;
        uint32_t prev_version=0;
        uint32_t version=0;
        Type type=TYPE_NONE;
        bool chain_forward=false;
        bool chain_backward=false;
    };

    // data
    Cache cache;
    Text text;
    Selection selection;
    Cursor cursor = {};
    HashSet<UIString> completion_prefixes;
    Vector<ScriptCodeCompletionOption> completion_sources;
    Vector<ScriptCodeCompletionOption> completion_options;
    ScriptCodeCompletionOption completion_current;
    Rect2i completion_rect;
    String completion_hint;
    String completion_base;
    int completion_index;
    int completion_line_ofs=0;
    int completion_hint_offset;

    uint32_t version = 0;
    uint32_t saved_version = 0;

    UIString space_indent="    ";
    UIString cut_copy_line;
    UIString ime_text;
    UIString highlighted_word;
    UIString search_text;
    uint32_t search_flags;

    Point2 ime_selection;

    TextEdit *m_owner;
    Timer *click_select_held;

    Map<int, int> color_region_cache;
    //syntax coloring
    SyntaxHighlighter *syntax_highlighter;
    Vector<TextColorRegion> color_regions;

    eastl::unordered_map<UIString, Color> keywords;
    eastl::unordered_map<UIString, Color> member_keywords;
    Map<int, Map<int, TextEdit::HighlighterInfo> > syntax_highlighting_cache;

    TextOperation current_op;

    Vector<TextOperation> undo_stack;
    /* Line and character position. */
    struct LineDrawingCache {
        int y_offset = 0;
        Vector<int> first_visible_char;
        Vector<int> last_visible_char;
    };

    Map<int, LineDrawingCache> line_drawing_cache;
    int undo_stack_pos = -1;
    int undo_stack_max_size;

    int wrap_at=0;
    int wrap_right_offset=10;

    bool text_changed_dirty=false;
    bool cursor_changed_dirty=false;

    bool next_operation_is_complex=false;
    bool undo_enabled=true;
    bool selecting_enabled=true;
    bool deselect_on_focus_loss_enabled=true;
    bool popup_show = false;
    bool hiding_enabled = false;
    bool wrap_enabled=false;
    bool setting_text=false;
    bool setting_row=false;

    bool completion_enabled=false;
    bool completion_active=false;
    bool completion_forced;

    bool dragging_selection = false;

    bool hovering_minimap = false;

    PrivateData(TextEdit *owner,int indent_size) : m_owner(owner) {
        text.set_indent_size(indent_size);
        text.clear();
        text.set_color_regions(&color_regions);
        current_op.type = TextOperation::TYPE_NONE;
        current_op.version = 0;
        undo_stack_max_size = T_GLOBAL_GET<int>("gui/common/text_edit_undo_stack_max_size");
    }
    void _clear() {
        clear_undo_history();
        text.clear();
        cursor.column = 0;
        cursor.line = 0;
        cursor.x_ofs = 0;
        cursor.line_ofs = 0;
        cursor.wrap_ofs = 0;
        cursor.last_fit_x = 0;
        selection.active = false;
    }
    Map<int, TextEdit::HighlighterInfo> _get_line_syntax_highlighting(TextEdit *te,int p_line);
    void _update_caches(TextEdit *te) {

        cache.style_normal = te->get_theme_stylebox("normal");
        cache.style_focus = te->get_theme_stylebox("focus");
        cache.style_readonly = te->get_theme_stylebox("read_only");
        cache.completion_background_color = te->get_theme_color("completion_background_color");
        cache.completion_selected_color = te->get_theme_color("completion_selected_color");
        cache.completion_existing_color = te->get_theme_color("completion_existing_color");
        cache.completion_font_color = te->get_theme_color("completion_font_color");
        cache.font = te->get_theme_font("font");
        cache.caret_color = te->get_theme_color("caret_color");
        cache.caret_background_color = te->get_theme_color("caret_background_color");
        cache.line_number_color = te->get_theme_color("line_number_color");
        cache.safe_line_number_color = te->get_theme_color("safe_line_number_color");
        cache.font_color = te->get_theme_color("font_color");
        cache.font_color_selected = te->get_theme_color("font_color_selected");
        cache.font_color_readonly = te->get_theme_color("font_color_readonly");
        cache.keyword_color = te->get_theme_color("keyword_color");
        cache.control_flow_keyword_color = te->get_theme_color("control_flow_keyword_color");
        cache.function_color = te->get_theme_color("function_color");
        cache.member_variable_color = te->get_theme_color("member_variable_color");
        cache.number_color = te->get_theme_color("number_color");
        cache.selection_color = te->get_theme_color("selection_color");
        cache.mark_color = te->get_theme_color("mark_color");
        cache.current_line_color = te->get_theme_color("current_line_color");
        cache.line_length_guideline_color = te->get_theme_color("line_length_guideline_color");
        cache.bookmark_color = te->get_theme_color("bookmark_color");
        cache.breakpoint_color = te->get_theme_color("breakpoint_color");
        cache.executing_line_color = te->get_theme_color("executing_line_color");
        cache.code_folding_color = te->get_theme_color("code_folding_color");
        cache.brace_mismatch_color = te->get_theme_color("brace_mismatch_color");
        cache.word_highlighted_color = te->get_theme_color("word_highlighted_color");
        cache.search_result_color = te->get_theme_color("search_result_color");
        cache.search_result_border_color = te->get_theme_color("search_result_border_color");
        cache.symbol_color = te->get_theme_color("symbol_color");
        cache.background_color = te->get_theme_color("background_color");
    #ifdef TOOLS_ENABLED
        cache.line_spacing = te->get_theme_constant("line_spacing") * EDSCALE;
    #else
        cache.line_spacing = te->get_theme_constant("line_spacing");
    #endif
        cache.row_height = cache.font->get_height() + cache.line_spacing;
        cache.tab_icon = te->get_theme_icon("tab");
        cache.space_icon = te->get_theme_icon("space");
        cache.folded_icon = te->get_theme_icon("folded");
        cache.can_fold_icon = te->get_theme_icon("fold");
        cache.folded_eol_icon = te->get_theme_icon("GuiEllipsis", "EditorIcons");
        cache.executing_icon = te->get_theme_icon("MainPlay", "EditorIcons");

        if (syntax_highlighter) {
            syntax_highlighter->_update_cache();
        }
    }
    int get_char_count() {

        int totalsize = 0;

        for (size_t i = 0; i < text.size(); i++) {

            if (i > 0)
                totalsize++; // Include \n.
            totalsize += text[i].length();
        }

        return totalsize; // Omit last \n.
    }
    void _line_edited_from(int p_line) {
        size_t cache_size = color_region_cache.size();
        for (int i = p_line; i < cache_size; i++) {
            color_region_cache.erase(i);
        }

        if (!syntax_highlighting_cache.empty()) {
            cache_size = syntax_highlighting_cache.rbegin()->first;
            for (int i = p_line - 1; i <= cache_size; i++) {
                if (syntax_highlighting_cache.contains(i)) {
                    syntax_highlighting_cache.erase(i);
                }
            }
        }
    }
    int _is_line_in_region(int p_line){
        // Do we have in cache?
        if (color_region_cache.contains(p_line)) {
            return color_region_cache[p_line];
        }

        // If not find the closest line we have.
        int previous_line = p_line - 1;
        for (; previous_line > -1; previous_line--) {
            if (color_region_cache.contains(p_line)) {
                break;
            }
        }

        // Calculate up to line we need and update the cache along the way.
        int in_region = color_region_cache[previous_line];
        if (previous_line == -1) {
            in_region = -1;
        }
        for (int i = previous_line; i < p_line; i++) {
            const Map<int, TextColorRegionInfo> &cri_map = _get_line_color_region_info(i);
            for (const eastl::pair<const int,TextColorRegionInfo> &E : cri_map) {
                const TextColorRegionInfo &cri = E.second;
                if (in_region == -1) {
                    if (!cri.end) {
                        in_region = cri.region;
                    }
                } else if (in_region == cri.region && !_get_color_region(cri.region).line_only) {
                    if (cri.end || _get_color_region(cri.region).eq) {
                        in_region = -1;
                    }
                }
            }

            if (in_region >= 0 && _get_color_region(in_region).line_only) {
                in_region = -1;
            }

            color_region_cache[i + 1] = in_region;
        }
        return in_region;
    }
    Map<int, TextColorRegionInfo> _get_line_color_region_info(int p_line) const {
        if (p_line < 0 || p_line > text.size() - 1) {
            return Map<int, TextColorRegionInfo>();
        }
        return text.get_color_region_info(p_line);
    }
    void clear_colors() {
        keywords.clear();
        member_keywords.clear();
        color_regions.clear();
        color_region_cache.clear();
        syntax_highlighting_cache.clear();
        text.clear_width_cache();

    }
    TextColorRegion _get_color_region(int p_region) {
        if (p_region < 0 || p_region >= color_regions.size()) {
            return TextColorRegion();
        }
        return color_regions[p_region];
    }

    void _clear_redo() {

        if (undo_stack_pos == -1)
            return; // Nothing to clear.

        _push_current_op();
        undo_stack.erase(undo_stack.begin()+undo_stack_pos, undo_stack.end());
        undo_stack_pos = -1;
    }

    void _push_current_op() {

        if (current_op.type == TextOperation::TYPE_NONE)
            return; // Nothing to do.

        if (next_operation_is_complex) {
            current_op.chain_forward = true;
            next_operation_is_complex = false;
        }

        undo_stack.push_back(current_op);
        current_op.type = TextOperation::TYPE_NONE;
        current_op.text = "";
        current_op.chain_forward = false;

        if (undo_stack.size() > undo_stack_max_size) {
            undo_stack.pop_front();
        }
    }
    void _do_text_op(const TextOperation &p_op, bool p_reverse) {

        ERR_FAIL_COND(p_op.type == TextOperation::TYPE_NONE);

        bool insert = p_op.type == TextOperation::TYPE_INSERT;
        if (p_reverse)
            insert = !insert;

        if (insert) {

            int check_line;
            int check_column;
            _base_insert_text(p_op.from_line, p_op.from_column, p_op.text, check_line, check_column);
            ERR_FAIL_COND(check_line != p_op.to_line); // BUG.
            ERR_FAIL_COND(check_column != p_op.to_column); // BUG.
        } else {

            _base_remove_text(p_op.from_line, p_op.from_column, p_op.to_line, p_op.to_column);
        }
    }

    void undo() {

        _push_current_op();

        if (undo_stack_pos == -1) {

            if (undo_stack.empty())
                return; // Nothing to undo.

            undo_stack_pos = undo_stack.size()-1;

        } else if (undo_stack_pos == 0)
            return; // At the bottom of the undo stack.
        else
            --undo_stack_pos;

        deselect();

        TextOperation op = undo_stack[undo_stack_pos];
        _do_text_op(op, true);

        current_op.version = op.prev_version;
        if (undo_stack[undo_stack_pos].chain_backward) {
            while (true) {
                ERR_BREAK(undo_stack_pos==0);
                --undo_stack_pos;
                op = undo_stack[undo_stack_pos];
                _do_text_op(op, true);
                current_op.version = op.prev_version;
                if (undo_stack[undo_stack_pos].chain_forward) {
                    break;
                }
            }
        }
        if (op.type != TextOperation::TYPE_INSERT && (op.from_line != op.to_line || op.to_column != op.from_column + 1)) {
            select(op.from_line, op.from_column, op.to_line, op.to_column);
        }

        m_owner->_update_scrollbars();

        if (undo_stack[undo_stack_pos].type == TextOperation::TYPE_REMOVE) {
            cursor_set_line(undo_stack[undo_stack_pos].to_line, false);
            cursor_set_column(undo_stack[undo_stack_pos].to_column);
            _cancel_code_hint();
        } else {
            cursor_set_line(undo_stack[undo_stack_pos].from_line, false);
            cursor_set_column(undo_stack[undo_stack_pos].from_column);
        }
        m_owner->update();
    }
    void redo() {

        _push_current_op();

        if (undo_stack_pos == -1 || undo_stack_pos==undo_stack.size())
            return; // Nothing to do.

        deselect();

        TextOperation op = undo_stack[undo_stack_pos];
        _do_text_op(op, false);
        current_op.version = op.version;
        if (undo_stack[undo_stack_pos].chain_forward) {

            while (true) {
                ERR_BREAK(undo_stack_pos+1 >= undo_stack.size());
                ++undo_stack_pos;
                op = undo_stack[undo_stack_pos];
                _do_text_op(op, false);
                current_op.version = op.version;
                if (undo_stack[undo_stack_pos].chain_backward)
                    break;
            }
        }

        m_owner->_update_scrollbars();
        cursor_set_line(undo_stack[undo_stack_pos].to_line, false);
        cursor_set_column(undo_stack[undo_stack_pos].to_column);
        ++undo_stack_pos;
        m_owner->update();
    }

    void clear_undo_history() {

        saved_version = 0;
        current_op.type = TextOperation::TYPE_NONE;
        undo_stack_pos = -1;
        undo_stack.clear();
    }

    void begin_complex_operation() {
        _push_current_op();
        next_operation_is_complex = true;
    }

    void end_complex_operation() {

        _push_current_op();
        ERR_FAIL_COND(undo_stack.empty());

        if (undo_stack.back().chain_forward) {
            undo_stack.back().chain_forward = false;
            return;
        }

        undo_stack.back().chain_backward = true;
    }

    void paste() {

        UIString clipboard = StringUtils::from_utf8(OS::get_singleton()->get_clipboard());

        begin_complex_operation();
        if (selection.active) {

            selection.active = false;
            selection.selecting_mode = Selection::MODE_NONE;
            _remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
            cursor_set_line(selection.from_line, false);
            cursor_set_column(selection.from_column);

        } else if (!cut_copy_line.isEmpty() && cut_copy_line == clipboard) {

            cursor_set_column(0);
            UIString ins("\n");
            clipboard += ins;
        }

        _insert_text_at_cursor(clipboard);
        end_complex_operation();

        m_owner->update();
    }

    void _base_insert_text(int p_line, int p_char, const UIString &p_text, int &r_end_line, int &r_end_column) {

        // Save for undo.
        ERR_FAIL_INDEX(p_line, text.size());
        ERR_FAIL_COND(p_char < 0);

        /* STEP 1: Remove \r from source text and separate in substrings. */
        UIString without_slash_r(p_text);
        without_slash_r.replace("\r", "");
        auto substrings = StringUtils::split(without_slash_r,'\n');

        /* STEP 2: Fire breakpoint_toggled signals. */

        // Is this just a new empty line?
        bool shift_first_line = p_char == 0 && without_slash_r == "\n";

        int i = p_line + !shift_first_line;
        int lines = substrings.size() - 1;
        for (; i < text.size(); i++) {
            if (text.is_breakpoint(i)) {
                if ((i - lines < p_line || !text.is_breakpoint(i - lines)) ||
                        (i - lines == p_line && !shift_first_line))
                    m_owner->emit_signal("breakpoint_toggled", i);
                if (i + lines >= text.size() || !text.is_breakpoint(i + lines))
                    m_owner->emit_signal("breakpoint_toggled", i + lines);
            }
        }

        /* STEP 3: Add spaces if the char is greater than the end of the line. */
        while (p_char > text[p_line].length()) {

            text.set(p_line, text[p_line] + ' ');
        }

        /* STEP 4: Separate dest string in pre and post text. */

        UIString preinsert_text = StringUtils::substr(text[p_line],0, p_char);
        UIString postinsert_text = StringUtils::substr(text[p_line],p_char, text[p_line].size());

        for (int j = 0; j < substrings.size(); j++) {
            // Insert the substrings.

            if (j == 0) {

                text.set(p_line, preinsert_text + substrings[j]);
            } else {

                text.insert(p_line + j, substrings[j]);
            }

            if (j == substrings.size() - 1) {

                text.set(p_line + j, text[p_line + j] + postinsert_text);
            }
        }

        if (shift_first_line) {
            text.set_breakpoint(p_line + 1, text.is_breakpoint(p_line));
            text.set_hidden(p_line + 1, text.is_hidden(p_line));
            if (text.has_info_icon(p_line)) {
                text.set_info_icon(p_line + 1, text.get_info_icon(p_line), text.get_info(p_line));
            }
            text.set_breakpoint(p_line, false);
            text.set_hidden(p_line, false);
            text.set_info_icon(p_line, Ref<Texture>(), StringName());
        }

        text.set_line_wrap_amount(p_line, -1);

        r_end_line = p_line + substrings.size() - 1;
        r_end_column = text[r_end_line].length() - postinsert_text.length();

        if (!text_changed_dirty && !setting_text) {
            if (m_owner->is_inside_tree()) {
                m_owner->call_deferred([owner=m_owner] { owner->_text_changed_emit(); });
            }
            text_changed_dirty = true;
        }
        _line_edited_from(p_line);
    }

    UIString _base_get_text(int p_from_line, int p_from_column, int p_to_line, int p_to_column) const {

        ERR_FAIL_INDEX_V(p_from_line, text.size(), UIString());
        ERR_FAIL_INDEX_V(p_from_column, text[p_from_line].length() + 1, UIString());
        ERR_FAIL_INDEX_V(p_to_line, text.size(), UIString());
        ERR_FAIL_INDEX_V(p_to_column, text[p_to_line].length() + 1, UIString());
        ERR_FAIL_COND_V(p_to_line < p_from_line, UIString()); // 'from > to'.
        ERR_FAIL_COND_V(p_to_line == p_from_line && p_to_column < p_from_column, UIString()); // 'from > to'.;

        UIString ret;

        for (int i = p_from_line; i <= p_to_line; i++) {

            int begin = (i == p_from_line) ? p_from_column : 0;
            int end = (i == p_to_line) ? p_to_column : text[i].length();

            if (i > p_from_line)
                ret += UIString("\n");
            ret += StringUtils::substr(text[i],begin, end - begin);
        }

        return ret;
    }

    void _base_remove_text(int p_from_line, int p_from_column, int p_to_line, int p_to_column) {

        ERR_FAIL_INDEX(p_from_line, text.size());
        ERR_FAIL_INDEX(p_from_column, text[p_from_line].length() + 1);
        ERR_FAIL_INDEX(p_to_line, text.size());
        ERR_FAIL_INDEX(p_to_column, text[p_to_line].length() + 1);
        ERR_FAIL_COND(p_to_line < p_from_line); // 'from > to'.
        ERR_FAIL_COND(p_to_line == p_from_line && p_to_column < p_from_column); // 'from > to'.

        UIString pre_text = StringUtils::substr(text[p_from_line],0, p_from_column);
        UIString post_text = StringUtils::substr(text[p_to_line],p_to_column, text[p_to_line].length());

        int lines = p_to_line - p_from_line;

        for (int i = p_from_line + 1; i < text.size(); i++) {
            if (text.is_breakpoint(i)) {
                if (i + lines >= text.size() || !text.is_breakpoint(i + lines))
                    m_owner->emit_signal("breakpoint_toggled", i);
                if (i > p_to_line && (i - lines < 0 || !text.is_breakpoint(i - lines)))
                    m_owner->emit_signal("breakpoint_toggled", i - lines);
            }
        }

        for (int i = p_from_line; i < p_to_line; i++) {
            text.remove(p_from_line + 1);
        }
        text.set(p_from_line, pre_text + post_text);

        text.set_line_wrap_amount(p_from_line, -1);

        if (!text_changed_dirty && !setting_text) {
            if (m_owner->is_inside_tree()) {
                m_owner->call_deferred([owner = m_owner] { owner->_text_changed_emit(); });
            }
            text_changed_dirty = true;
        }
        _line_edited_from(p_from_line);
    }
    void _insert_text(
            int p_line, int p_char, const UIString &p_text, int *r_end_line = nullptr, int *r_end_char = nullptr) {

        if (!setting_text)
            start_stop_idle_detection(m_owner,true);

        if (undo_enabled) {
            _clear_redo();
        }

        int retline, retchar;
        _base_insert_text(p_line, p_char, p_text, retline, retchar);
        if (r_end_line)
            *r_end_line = retline;
        if (r_end_char)
            *r_end_char = retchar;

        if (!undo_enabled)
            return;

        /* UNDO!! */
        TextOperation op;
        op.type = TextOperation::TYPE_INSERT;
        op.from_line = p_line;
        op.from_column = p_char;
        op.to_line = retline;
        op.to_column = retchar;
        op.text = p_text;
        op.version = ++version;
        op.chain_forward = false;
        op.chain_backward = false;

        // See if it should just be set as current op.
        if (current_op.type != op.type) {
            op.prev_version = m_owner->get_version();
            _push_current_op();
            current_op = op;

            return; // Set as current op, return.
        }
        // See if it can be merged.
        if (current_op.to_line != p_line || current_op.to_column != p_char) {
            op.prev_version = m_owner->get_version();
            _push_current_op();
            current_op = op;
            return; // Set as current op, return.
        }
        // Merge current op.

        current_op.text += p_text;
        current_op.to_column = retchar;
        current_op.to_line = retline;
        current_op.version = op.version;
    }
    void _remove_text(int p_from_line, int p_from_column, int p_to_line, int p_to_column) {

        if (!setting_text )
            start_stop_idle_detection(m_owner,true);

        UIString text;
        if (undo_enabled) {
            _clear_redo();
            text = _base_get_text(p_from_line, p_from_column, p_to_line, p_to_column);
        }

        _base_remove_text(p_from_line, p_from_column, p_to_line, p_to_column);

        if (!undo_enabled)
            return;

        /* UNDO! */
        TextOperation op;
        op.type = TextOperation::TYPE_REMOVE;
        op.from_line = p_from_line;
        op.from_column = p_from_column;
        op.to_line = p_to_line;
        op.to_column = p_to_column;
        op.text = text;
        op.version = ++version;
        op.chain_forward = false;
        op.chain_backward = false;

        // See if it should just be set as current op.
        if (current_op.type != op.type) {
            op.prev_version = m_owner->get_version();
            _push_current_op();
            current_op = op;
            return; // Set as current op, return.
        }
        // See if it can be merged.
        if (current_op.from_line == p_to_line && current_op.from_column == p_to_column) {
            // Backspace or similar.
            current_op.text = text + current_op.text;
            current_op.from_line = p_from_line;
            current_op.from_column = p_from_column;
            return; // Update current op.
        }

        op.prev_version = m_owner->get_version();
        _push_current_op();
        current_op = op;
    }
    void _insert_text_at_cursor(const UIString &p_text) {

        int new_column, new_line;
        _insert_text(cursor.line, cursor.column, p_text, &new_line, &new_column);
        m_owner->_update_scrollbars();
        cursor_set_line(new_line, false);
        cursor_set_column(new_column);

        m_owner->update();
    }

    void _consume_backspace_for_pair_symbol(int prev_line, int prev_column) {

        bool remove_right_symbol = false;

        if (cursor.column < text[cursor.line].length() && cursor.column > 0) {

            CharType left_char = text[cursor.line][cursor.column - 1];
            CharType right_char = text[cursor.line][cursor.column];

            if (right_char == _get_right_pair_symbol(left_char)) {
                remove_right_symbol = true;
            }
        }
        if (remove_right_symbol) {
            _remove_text(prev_line, prev_column, cursor.line, cursor.column + 1);
        } else {
            _remove_text(prev_line, prev_column, cursor.line, cursor.column);
        }
    }
    int cursor_get_column() const { return cursor.column; }

    int cursor_get_line() const { return cursor.line; }
    void _consume_pair_symbol(CharType ch) {

        int cursor_position_to_move = m_owner->cursor_get_column() + 1;

        UIString ch_single = QString(1,ch) ;
        CharType ch_single_pair[2] = { _get_right_pair_symbol(ch), 0 };
        CharType ch_pair[3] = { ch, _get_right_pair_symbol(ch), 0 };

        if (m_owner->is_selection_active()) {

            int new_column, new_line;

            m_owner->begin_complex_operation();
            _insert_text(get_selection_from_line(), get_selection_from_column(), UIString(ch), &new_line, &new_column);

            int to_col_offset = 0;
            if (get_selection_from_line() == get_selection_to_line())
                to_col_offset = 1;

            _insert_text(get_selection_to_line(), get_selection_to_column() + to_col_offset,
                    QString::fromRawData(ch_single_pair, 1), &new_line, &new_column);
            end_complex_operation();

            cursor_set_line(get_selection_to_line());
            cursor_set_column(get_selection_to_column() + to_col_offset);

            deselect();
            m_owner->update();
            return;
        }

        if ((ch == '\'' || ch == '"') && cursor_get_column() > 0 &&
                _te_is_text_char(text[cursor.line][cursor_get_column() - 1]) &&
                !_is_pair_right_symbol(text[cursor.line][cursor_get_column()])) {
            insert_text_at_cursor(ch_single);
            cursor_set_column(cursor_position_to_move);
            return;
        }

        if (cursor_get_column() < text[cursor.line].length()) {
            if (_te_is_text_char(text[cursor.line][cursor_get_column()])) {
                insert_text_at_cursor(ch_single);
                cursor_set_column(cursor_position_to_move);
                return;
            }
            if (_is_pair_right_symbol(ch) && text[cursor.line][cursor_get_column()] == ch) {
                cursor_set_column(cursor_position_to_move);
                return;
            }
        }

        UIString line = text[cursor.line];

        bool in_single_quote = false;
        bool in_double_quote = false;
        bool found_comment = false;

        int c = 0;
        while (c < line.length()) {
            if (line[c] == '\\') {
                c++; // Skip quoted anything.

                if (cursor.column == c) {
                    break;
                }
            } else if (!in_single_quote && !in_double_quote && line[c] == '#') {
                found_comment = true;
                break;
            } else {
                if (line[c] == '\'' && !in_double_quote) {
                    in_single_quote = !in_single_quote;
                } else if (line[c] == '"' && !in_single_quote) {
                    in_double_quote = !in_double_quote;
                }
            }

            c++;

            if (cursor.column == c) {
                break;
            }
        }
        // Do not need to duplicate quotes while in comments
        if (found_comment) {
            insert_text_at_cursor(ch_single);
            cursor_set_column(cursor_position_to_move);

            return;
        }

        //    Disallow inserting duplicated quotes while already in string
        if ((in_single_quote || in_double_quote) && (ch == '"' || ch == '\'')) {
            insert_text_at_cursor(ch_single);
            cursor_set_column(cursor_position_to_move);

            return;
        }
        insert_text_at_cursor(QString::fromRawData(ch_pair,2));
        cursor_set_column(cursor_position_to_move);
    }

    bool is_selection_active() const { return selection.active; }
    int get_selection_from_line() const {

        ERR_FAIL_COND_V(!selection.active, -1);
        return selection.from_line;
    }
    int get_selection_from_column() const {

        ERR_FAIL_COND_V(!selection.active, -1);
        return selection.from_column;
    }
    int get_selection_to_line() const {

        ERR_FAIL_COND_V(!selection.active, -1);
        return selection.to_line;
    }
    int get_selection_to_column() const {

        ERR_FAIL_COND_V(!selection.active, -1);
        return selection.to_column;
    }

    String get_selection_text() const {

        if (!selection.active)
            return String();

        return StringUtils::to_utf8(
                _base_get_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column));
    }
    String get_line(int line) const {

        if (line < 0 || line >= text.size())
            return String();

        return StringUtils::to_utf8(text[line]).data();
    }
    void cursor_set_line(int p_row, bool p_adjust_viewport = true, bool p_can_be_hidden = true, int p_wrap_index = 0) {

        if (setting_row)
            return;

        setting_row = true;
        if (p_row < 0)
            p_row = 0;

        if (p_row >= text.size())
            p_row = text.size() - 1;

        if (!p_can_be_hidden) {
            if (is_line_hidden(CLAMP<int>(p_row, 0, text.size() - 1))) {
                int move_down = num_lines_from(p_row, 1) - 1;
                if (p_row + move_down <= text.size() - 1 && !is_line_hidden(p_row + move_down)) {
                    p_row += move_down;
                } else {
                    int move_up = num_lines_from(p_row, -1) - 1;
                    if (p_row - move_up > 0 && !is_line_hidden(p_row - move_up)) {
                        p_row -= move_up;
                    } else {
                        WARN_PRINT(("Cursor set to hidden line " + itos(p_row) + " and there are no nonhidden lines."));
                    }
                }
            }
        }
        cursor.line = p_row;

        int n_col = m_owner->get_char_pos_for_line(cursor.last_fit_x, p_row, p_wrap_index);
        if (n_col != 0 && wrap_enabled && p_wrap_index < m_owner->get_line_wrap_count(p_row)) {
            Vector<UIString> rows = m_owner->get_wrap_rows_text(p_row);
            int row_end_col = 0;
            for (int i = 0; i < p_wrap_index + 1; i++) {
                row_end_col += rows[i].length();
            }
            if (n_col >= row_end_col)
                n_col -= 1;
        }
        cursor.column = n_col;

        if (p_adjust_viewport)
            m_owner->adjust_viewport_to_cursor();

        setting_row = false;

        if (!cursor_changed_dirty) {
            if (m_owner->is_inside_tree()) {
                m_owner->call_deferred([owner = m_owner] { owner->_cursor_changed_emit(); });
            }
            cursor_changed_dirty = true;
        }
    }
    void cursor_set_column(int p_col, bool p_adjust_viewport = true) {

        if (p_col < 0)
            p_col = 0;

        cursor.column = p_col;
        if (cursor.column > get_line(cursor.line).length())
            cursor.column = get_line(cursor.line).length();

        cursor.last_fit_x = m_owner->get_column_x_offset_for_line(cursor.column, cursor.line);

        if (p_adjust_viewport)
            m_owner->adjust_viewport_to_cursor();

        if (!cursor_changed_dirty) {
            if (m_owner->is_inside_tree()) {
                m_owner->call_deferred([owner = m_owner] { owner->_cursor_changed_emit(); });
            }
            cursor_changed_dirty = true;
        }
    }

    void _update_selection_mode_line() {
        selection.drag_attempt = false;
        dragging_selection = true;
        Point2 mp = m_owner->get_local_mouse_position();

        int row, col;
        m_owner->_get_mouse_pos(Point2i(mp.x, mp.y), row, col);

        col = 0;
        if (row < selection.selecting_line) {
            // Cursor is above us.
            cursor_set_line(row - 1, false);
            selection.selecting_column = text[selection.selecting_line].length();
        } else {
            // Cursor is below us.
            cursor_set_line(row + 1, false);
            selection.selecting_column = 0;
            col = text[row].length();
        }
        cursor_set_column(0);

        select(selection.selecting_line, selection.selecting_column, row, col);
        if (OS::get_singleton()->has_feature("primary_clipboard")) {
            OS::get_singleton()->set_clipboard_primary(get_selection_text());
        }
        m_owner->update();

        click_select_held->start();
    }
    void select(int p_from_line, int p_from_column, int p_to_line, int p_to_column) {
        if (!selecting_enabled)
            return;

        if (p_from_line < 0)
            p_from_line = 0;
        else if (p_from_line >= text.size())
            p_from_line = text.size() - 1;
        if (p_from_column >= text[p_from_line].length())
            p_from_column = text[p_from_line].length();
        if (p_from_column < 0)
            p_from_column = 0;

        if (p_to_line < 0)
            p_to_line = 0;
        else if (p_to_line >= text.size())
            p_to_line = text.size() - 1;
        if (p_to_column >= text[p_to_line].length())
            p_to_column = text[p_to_line].length();
        if (p_to_column < 0)
            p_to_column = 0;

        selection.from_line = p_from_line;
        selection.from_column = p_from_column;
        selection.to_line = p_to_line;
        selection.to_column = p_to_column;

        selection.active = true;

        if (selection.from_line == selection.to_line) {

            if (selection.from_column == selection.to_column) {

                selection.active = false;

            } else if (selection.from_column > selection.to_column) {

                selection.shiftclick_left = false;
                SWAP(selection.from_column, selection.to_column);
            } else {

                selection.shiftclick_left = true;
            }
        } else if (selection.from_line > selection.to_line) {

            selection.shiftclick_left = false;
            SWAP(selection.from_line, selection.to_line);
            SWAP(selection.from_column, selection.to_column);
        } else {

            selection.shiftclick_left = true;
        }
    }
    void select_all() {
        if (!selecting_enabled)
            return;

        if (text.size() == 1 && text[0].length() == 0)
            return;
        selection.active = true;
        selection.from_line = 0;
        selection.from_column = 0;
        selection.selecting_line = 0;
        selection.selecting_column = 0;
        selection.to_line = text.size() - 1;
        selection.to_column = text[selection.to_line].length();
        selection.selecting_mode = Selection::MODE_SHIFT;
        selection.shiftclick_left = true;
        cursor_set_line(selection.to_line, false);
        cursor_set_column(selection.to_column, false);
        m_owner->update();
    }
    void deselect() {
        selection.active = false;
        m_owner->update();
    }

    void insert_text_at_cursor(const UIString &p_text) {

        if (selection.active) {

            cursor_set_line(selection.from_line, false);
            cursor_set_column(selection.from_column);

            _remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
            selection.active = false;
            selection.selecting_mode = Selection::MODE_NONE;
        }

        _insert_text_at_cursor(p_text);
        m_owner->update();
    }
    void insert_at(const UIString &p_text, int at) {
        _insert_text(at, 0, p_text + "\n");
        if (cursor.line >= at) {
            // offset cursor when located after inserted line
            ++cursor.line;
        }
        if (is_selection_active()) {
            if (selection.from_line >= at) {
                // offset selection when located after inserted line
                ++selection.from_line;
                ++selection.to_line;
            } else if (selection.to_line >= at) {
                // extend selection that includes inserted line
                ++selection.to_line;
            }
        }
    }

    bool is_line_hidden(int p_line) const {

        ERR_FAIL_INDEX_V(p_line, text.size(), false);
        return text.is_hidden(p_line);
    }
    int num_lines_from(int p_line_from, int visible_amount) const {

        // Returns the number of lines (hidden and unhidden) from p_line_from to (p_line_from + visible_amount of
        // unhidden lines).
        ERR_FAIL_INDEX_V(p_line_from, text.size(), ABS(visible_amount));

        if (!hiding_enabled)
            return ABS(visible_amount);

        int num_visible = 0;
        int num_total = 0;
        if (visible_amount >= 0) {
            for (int i = p_line_from; i < text.size(); i++) {
                num_total++;
                if (!is_line_hidden(i)) {
                    num_visible++;
                }
                if (num_visible >= visible_amount)
                    break;
            }
        } else {
            visible_amount = ABS(visible_amount);
            for (int i = p_line_from; i >= 0; i--) {
                num_total++;
                if (!is_line_hidden(i)) {
                    num_visible++;
                }
                if (num_visible >= visible_amount)
                    break;
            }
        }
        return num_total;
    }

    void set_text(const UIString& p_text) {

        setting_text = true;
        if (!undo_enabled) {
            _clear();
            _insert_text_at_cursor(p_text);
        }

        if (undo_enabled) {
            cursor_set_line(0);
            cursor_set_column(0);

            m_owner->begin_complex_operation();
            int text_range=eastl::max<int>(0, text.size() - 1);
            _remove_text(0, 0, text_range, eastl::max<int>(get_line(text_range).size(), 0));
            _insert_text_at_cursor(p_text);
            end_complex_operation();
            selection.active = false;
        }

        cursor_set_line(0);
        cursor_set_column(0);

        m_owner->update();
        setting_text = false;
    }
    void set_line(int line,StringView _new_text) {
        UIString new_text(StringUtils::from_utf8(_new_text));
        if (line < 0 || line >= text.size())
            return;
        _remove_text(line, 0, line, text[line].length());
        _insert_text(line, 0, new_text);
        if (cursor.line == line) {
            cursor.column = MIN(cursor.column, new_text.length());
        }
        if (is_selection_active() && line == selection.to_line && selection.to_column > text[line].length()) {
            selection.to_column = text[line].length();
        }

    }
    void _cancel_code_hint() {

        completion_hint = "";
        m_owner->update();
    }

    void _cancel_completion() {

        if (!completion_active)
            return;

        completion_active = false;
        completion_forced = false;
        m_owner->update();
    }
    void completion_key_up() {
        if (completion_index > 0) {
            completion_index--;
        } else {
            completion_index = completion_options.size() - 1;
        }
        completion_current = completion_options[completion_index];

    }
    void completion_key_down() {
        if (completion_index < completion_options.size() - 1) {
            completion_index++;
        } else {
            completion_index = 0;
        }
        completion_current = completion_options[completion_index];

    }
    void completion_key_page_up() {
        completion_index -= m_owner->get_theme_constant("completion_lines");
        if (completion_index < 0)
            completion_index = 0;
        completion_current = completion_options[completion_index];
    }
    void completion_key_page_down() {
        completion_index += m_owner->get_theme_constant("completion_lines");
        if (completion_index >= completion_options.size())
            completion_index = completion_options.size() - 1;
        completion_current = completion_options[completion_index];
    }
    bool completion_key_home() {
        if(completion_index <=0)
            return false;
        completion_index = 0;
        completion_current = completion_options[completion_index];
        return true;
    }
    bool completion_key_end() {
        if (completion_index >= completion_options.size() - 1)
            return false;

        completion_index = completion_options.size() - 1;
        completion_current = completion_options[completion_index];
        return true;
    }
    static int _get_column_pos_of_word(
            const UIString &p_key, const UIString &p_search, uint32_t p_search_flags, int p_from_column) {
        int col = -1;

        if (p_key.length() > 0 && p_search.length() > 0) {
            if (p_from_column < 0 || p_from_column > p_search.length()) {
                p_from_column = 0;
            }

            while (col == -1 && p_from_column <= p_search.length()) {
                if (p_search_flags & TextEdit::SEARCH_MATCH_CASE) {
                    col = StringUtils::find(p_search,p_key, p_from_column);
                } else {
                    col = StringUtils::findn(p_search,p_key, p_from_column);
                }

                // Whole words only.
                if (col != -1 && p_search_flags & TextEdit::SEARCH_WHOLE_WORDS) {
                    p_from_column = col;

                    if (col > 0 && _te_is_text_char(p_search[col - 1])) {
                        col = -1;
                    } else if ((col + p_key.length()) < p_search.length() &&
                               _te_is_text_char(p_search[col + p_key.length()])) {
                        col = -1;
                    }
                }

                p_from_column += 1;
            }
        }
        return col;
    }
    PoolVector<int> _search_bind(StringView _key, uint32_t p_search_flags, int p_from_line, int p_from_column) const {
        UIString p_key(StringUtils::from_utf8(_key));
        int col, line;
        if (m_owner->search(p_key, p_search_flags, p_from_line, p_from_column, line, col)) {
            PoolVector<int> result;
            result.resize(2);
            result.set(TextEdit::SEARCH_RESULT_COLUMN, col);
            result.set(TextEdit::SEARCH_RESULT_LINE, line);
            return result;

        } else {

            return PoolVector<int>();
        }
    }
    bool has_undo() const {

        if (undo_stack_pos == -1) {
            int pending = current_op.type == TextOperation::TYPE_NONE ? 0 : 1;
            return undo_stack.size() + pending > 0;
        }
        return undo_stack_pos != 0;
    }

    bool has_redo() const {
        return undo_stack_pos != -1;
    }

public:
    void _update_selection_mode_pointer();
    void _update_selection_mode_word();
};

void PrivateData::_update_selection_mode_pointer() {
    selection.drag_attempt = false;
    dragging_selection = true;
    Point2 mp = m_owner->get_local_mouse_position();

    int row, col;
    m_owner->_get_mouse_pos(Point2i(mp.x, mp.y), row, col);

    m_owner->select(selection.selecting_line, selection.selecting_column, row, col);

    cursor_set_line(row, false);
    cursor_set_column(col);
    m_owner->update();

    click_select_held->start();
}

void PrivateData::_update_selection_mode_word() {
    selection.drag_attempt = false;
    dragging_selection = true;
    Point2 mp = m_owner->get_local_mouse_position();

    int row, col;
    m_owner->_get_mouse_pos(Point2i(mp.x, mp.y), row, col);

    UIString line = text[row];
    int beg = CLAMP(col, 0, line.length());
    // If its the first selection and on whitespace make sure we grab the word instead.
    if (!selection.active) {
        while (beg > 0 && line[beg] <= 32) {
            beg--;
        }
    }
    int end = beg;
    bool symbol = beg < line.length() && _is_symbol(line[beg]);

    // Get the word end and begin points.
    while (beg > 0 && line[beg - 1] > 32 && (symbol == _is_symbol(line[beg - 1]))) {
        beg--;
    }
    while (end < line.length() && line[end + 1] > 32 && (symbol == _is_symbol(line[end + 1]))) {
        end++;
    }
    if (end < line.length()) {
        end += 1;
    }

    // Initial selection.
    if (!selection.active) {
        select(row, beg, row, end);
        selection.selecting_column = beg;
        selection.selected_word_beg = beg;
        selection.selected_word_end = end;
        selection.selected_word_origin = beg;
        cursor_set_line(selection.to_line, false);
        cursor_set_column(selection.to_column);
    } else {
        if ((col <= selection.selected_word_origin && row == selection.selecting_line) ||
                row < selection.selecting_line) {
            selection.selecting_column = selection.selected_word_end;
            select(row, beg, selection.selecting_line, selection.selected_word_end);
            cursor_set_line(selection.from_line, false);
            cursor_set_column(selection.from_column);
        } else {
            selection.selecting_column = selection.selected_word_beg;
            select(selection.selecting_line, selection.selected_word_beg, row, end);
            cursor_set_line(selection.to_line, false);
            cursor_set_column(selection.to_column);
        }
    }

    if (OS::get_singleton()->has_feature("primary_clipboard")) {
        OS::get_singleton()->set_clipboard_primary(get_selection_text());
    }

    m_owner->update();

    click_select_held->start();
}

} // namespace

IMPL_GDCLASS(TextEdit)
VARIANT_ENUM_CAST(TextEdit::MenuItems);
VARIANT_ENUM_CAST(TextEdit::SearchFlags);
VARIANT_ENUM_CAST(TextEdit::SearchResult);

#define D() ((PrivateData *)m_priv)
void Text::set_font(const Ref<Font> &p_font) {

    font = p_font;
}

void Text::set_indent_size(int p_indent_size) {

    indent_size = p_indent_size;
}

void Text::_update_line_cache(uint32_t p_line) const {

    int w = 0;

    int len = text[p_line].data.length();
    const CharType *str = text[p_line].data.data();

    // Update width.

    for (int i = 0; i < len; i++) {
        w += get_char_width(str[i], str[i + 1], w);
    }

    text[p_line].width_cache = w;

    text[p_line].wrap_amount_cache = -1;

    // Update regions.

    text[p_line].region_info.clear();

    for (int i = 0; i < len; i++) {

        if (!_is_symbol(str[i]))
            continue;
        if (str[i] == '\\') {
            i++; // Skip quoted anything.
            continue;
        }

        int left = len - i;

        for (size_t j = 0; j < color_regions->size(); j++) {

            const TextColorRegion &cr = color_regions->operator[](j);

            /* BEGIN */

            int lr = cr.begin_key.length();
            const CharType *kc;
            bool match;

            if (lr != 0 && lr <= left) {
                kc = cr.begin_key.constData();

                match = true;
                for (int k = 0; k < lr; k++) {
                    if (kc[k] != str[i + k]) {
                        match = false;
                        break;
                    }
                }

                if (match) {

                    TextColorRegionInfo cri;
                    cri.end = false;
                    cri.region = j;
                    text[p_line].region_info[i] = cri;
                    i += lr - 1;

                    break;
                }
            }

            /* END */

            lr = cr.end_key.length();
            if (lr != 0 && lr <= left) {
                kc = cr.end_key.data();

                match = true;

                for (int k = 0; k < lr; k++) {
                    if (kc[k] != str[i + k]) {
                        match = false;
                        break;
                    }
                }

                if (match) {

                    TextColorRegionInfo cri;
                    cri.end = true;
                    cri.region = j;
                    text[p_line].region_info[i] = cri;
                    i += lr - 1;

                    break;
                }
            }
        }
    }
}

const Map<int, TextColorRegionInfo> &Text::get_color_region_info(int p_line) const {

    static Map<int, TextColorRegionInfo> cri;
    ERR_FAIL_INDEX_V(p_line, text.size(), cri);

    if (text[p_line].width_cache == -1) {
        _update_line_cache(p_line);
    }

    return text[p_line].region_info;
}

int Text::get_line_width(int p_line) const {

    ERR_FAIL_INDEX_V(p_line, text.size(), -1);

    if (text[p_line].width_cache == -1) {
        _update_line_cache(p_line);
    }

    return text[p_line].width_cache;
}

void Text::set_line_wrap_amount(int p_line, int p_wrap_amount) const {

    ERR_FAIL_INDEX(p_line, text.size());

    text[p_line].wrap_amount_cache = p_wrap_amount;
}

int Text::get_line_wrap_amount(int p_line) const {

    ERR_FAIL_INDEX_V(p_line, text.size(), -1);

    return text[p_line].wrap_amount_cache;
}

void Text::clear_width_cache() {

    for (int i = 0; i < text.size(); i++) {
        text[i].width_cache = -1;
    }
}

void Text::clear_wrap_cache() {

    for (int i = 0; i < text.size(); i++) {
        text[i].wrap_amount_cache = -1;
    }
}

void Text::clear() {

    text.clear();
    insert(0, UIString());
}

int Text::get_max_width(bool p_exclude_hidden) const {
    // Quite some work, but should be fast enough.

    int max = 0;
    for (int i = 0; i < text.size(); i++) {
        if (!p_exclude_hidden || !is_hidden(i))
            max = M_MAX(max, get_line_width(i));
    }
    return max;
}

void Text::set(int p_line, const UIString &p_text) {

    ERR_FAIL_INDEX(p_line, text.size());

    text[p_line].width_cache = -1;
    text[p_line].wrap_amount_cache = -1;
    text[p_line].data = p_text;
}

void Text::insert(int p_at, const UIString &p_text) {

    Line line;
    line.marked = false;
    line.safe = false;
    line.breakpoint = false;
    line.bookmark = false;
    line.hidden = false;
    line.has_info = false;
    line.width_cache = -1;
    line.wrap_amount_cache = -1;
    line.data = p_text;
    text.insert(text.begin() + p_at, line);
}
void Text::remove(int p_at) {

    text.erase(text.begin() + p_at);
}

int Text::get_char_width(CharType c, CharType next_c, int px) const {

    int tab_w = font->get_char_size(' ').width * indent_size;
    int w = 0;

    if (c == '\t') {

        int left = px % tab_w;
        if (left == 0)
            w = tab_w;
        else
            w = tab_w - px % tab_w; // Is right.
    } else {

        w = font->get_char_size(c, next_c).width;
    }
    return w;
}

void TextEdit::_update_scrollbars() {

    Size2 size = get_size();
    Size2 hmin = h_scroll->get_combined_minimum_size();
    Size2 vmin = v_scroll->get_combined_minimum_size();

    v_scroll->set_begin(Point2(size.width - vmin.width, D()->cache.style_normal->get_margin(Margin::Top)));

    h_scroll->set_begin(Point2(0, size.height - hmin.height));
    v_scroll->set_end(Point2(size.width, size.height - D()->cache.style_normal->get_margin(Margin::Top) -
                                                                                           D()->cache.style_normal->get_margin(Margin::Bottom)));

    h_scroll->set_end(Point2(size.width - vmin.width, size.height));

    int visible_rows = get_visible_rows();
    int total_rows = get_total_visible_rows();
    if (scroll_past_end_of_file_enabled) {
        total_rows += visible_rows - 1;
    }

    int visible_width = size.width - D()->cache.style_normal->get_minimum_size().width;
    int total_width = D()->text.get_max_width(true) + vmin.x;

    if (line_numbers)
        total_width += D()->cache.line_number_w;

    if (draw_breakpoint_gutter || draw_bookmark_gutter) {
        total_width += D()->cache.breakpoint_gutter_width;
    }

    if (draw_info_gutter) {
        total_width += D()->cache.info_gutter_width;
    }

    if (draw_fold_gutter) {
        total_width += D()->cache.fold_gutter_width;
    }

    if (draw_minimap) {
        total_width += D()->cache.minimap_width;
    }

    updating_scrolls = true;

    if (total_rows > visible_rows) {

        v_scroll->show();
        v_scroll->set_max(total_rows + get_visible_rows_offset());
        v_scroll->set_page(visible_rows + get_visible_rows_offset());
        if (smooth_scroll_enabled) {
            v_scroll->set_step(0.25);
        } else {
            v_scroll->set_step(1);
        }
        set_v_scroll(get_v_scroll());

    } else {

        D()->cursor.line_ofs = 0;
        D()->cursor.wrap_ofs = 0;
        v_scroll->set_value(0);
        v_scroll->set_max(0);
        v_scroll->hide();
    }

    if (total_width > visible_width && !is_wrap_enabled()) {

        h_scroll->show();
        h_scroll->set_max(total_width);
        h_scroll->set_page(visible_width);
        if (D()->cursor.x_ofs > (total_width - visible_width))
            D()->cursor.x_ofs = (total_width - visible_width);
        if (fabs(h_scroll->get_value() - (double)D()->cursor.x_ofs) >= 1) {
            h_scroll->set_value(D()->cursor.x_ofs);
        }

    } else {

        D()->cursor.x_ofs = 0;
        h_scroll->set_value(0);
        h_scroll->set_max(0);
        h_scroll->hide();
    }

    updating_scrolls = false;
}

void TextEdit::_click_selection_held() {
    using Selection = PrivateData::Selection;
    // Warning: is_mouse_button_pressed(BUTTON_LEFT) returns false for double+ clicks, so this doesn't work for
    // MODE_WORD and MODE_LINE. However, moving the mouse triggers _gui_input, which calls these functions too, so
    // that's not a huge problem. I'm unsure if there's an actual fix that doesn't have a ton of side effects.
    if (Input::get_singleton()->is_mouse_button_pressed(BUTTON_LEFT) &&
            D()->selection.selecting_mode != Selection::MODE_NONE) {
        switch (D()->selection.selecting_mode) {
            case Selection::MODE_POINTER: {
                D()->_update_selection_mode_pointer();
            } break;
            case Selection::MODE_WORD: {
                D()->_update_selection_mode_word();
            } break;
            case Selection::MODE_LINE: {
                D()->_update_selection_mode_line();
            } break;
            default: {
                break;
            }
        }
    } else {
        D()->click_select_held->stop();
    }
}

void TextEdit::_update_minimap_hover() {
    const Point2 mp = get_local_mouse_position();
    const int xmargin_end = get_size().width - D()->cache.style_normal->get_margin(Margin::Right);

    const bool hovering_sidebar = mp.x > xmargin_end - minimap_width && mp.x < xmargin_end;
    if (!hovering_sidebar) {
        if (D()->hovering_minimap) {
            // Only redraw if the hovering status changed.
            D()->hovering_minimap = false;
            update();
        }

        // Return early to avoid running the operations below when not needed.
        return;
    }

    int row;
    _get_minimap_mouse_row(Point2i(mp.x, mp.y), row);

    const bool new_hovering_minimap = row >= get_first_visible_line() && row <= get_last_full_visible_line();
    if (new_hovering_minimap != D()->hovering_minimap) {
        // Only redraw if the hovering status changed.
        D()->hovering_minimap = new_hovering_minimap;
        update();
    }
}

void TextEdit::_update_minimap_click() {
    Point2 mp = get_local_mouse_position();

    int xmargin_end = get_size().width - D()->cache.style_normal->get_margin(Margin::Right);
    if (!dragging_minimap && (mp.x < xmargin_end - minimap_width || mp.y > xmargin_end)) {
        minimap_clicked = false;
        return;
    }
    minimap_clicked = true;
    dragging_minimap = true;

    int row;
    _get_minimap_mouse_row(Point2i(mp.x, mp.y), row);
    if (row >= get_first_visible_line() && (row < get_last_full_visible_line() || row >= (D()->text.size() - 1))) {
        minimap_scroll_ratio = v_scroll->get_as_ratio();
        minimap_scroll_click_pos = mp.y;
        can_drag_minimap = true;
        return;
    }

    int wi;
    int first_line = row - num_lines_from_rows(row, 0, -get_visible_rows() / 2, wi) + 1;
    double delta = get_scroll_pos_for_line(first_line, wi) - get_v_scroll();
    if (delta < 0) {
        _scroll_up(-delta);
    } else {
        _scroll_down(delta);
    }
}
void TextEdit::_update_minimap_drag() {

    if (!can_drag_minimap) {
        return;
    }

    int control_height = _get_control_height();
    int scroll_height = v_scroll->get_max() * (minimap_char_size.y + minimap_line_spacing);
    if (control_height > scroll_height) {
        control_height = scroll_height;
    }
    Point2 mp = get_local_mouse_position();
    double diff = (mp.y - minimap_scroll_click_pos) / control_height;
    v_scroll->set_as_ratio(minimap_scroll_ratio + diff);
}
void TextEdit::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {

            _update_caches();
            if (D()->cursor_changed_dirty) {
                MessageQueue::get_singleton()->push_call(get_instance_id(), [this] { _cursor_changed_emit();} );
            }
            if (D()->text_changed_dirty) {
                MessageQueue::get_singleton()->push_call(get_instance_id(), [this] { _text_changed_emit(); });
            }
            _update_wrap_at();
        } break;
        case NOTIFICATION_RESIZED: {

            _update_scrollbars();
            _update_wrap_at();
        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {
            if (is_visible()) {
                call_deferred([this]() {
                    _update_scrollbars();
                    _update_wrap_at();
                });
            }
        } break;
        case NOTIFICATION_THEME_CHANGED: {

            _update_caches();
            _update_wrap_at();
            D()->syntax_highlighting_cache.clear();
        } break;
        case MainLoop::NOTIFICATION_WM_FOCUS_IN: {
            window_has_focus = true;
            draw_caret = true;
            update();
        } break;
        case MainLoop::NOTIFICATION_WM_FOCUS_OUT: {
            window_has_focus = false;
            draw_caret = false;
            update();
        } break;
        case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
            if (scrolling && get_v_scroll() != target_v_scroll) {
                double target_y = target_v_scroll - get_v_scroll();
                double dist = sqrt(target_y * target_y);
                // To ensure minimap is responsive override the speed setting.
                double vel = ((target_y / dist) * ((minimap_clicked) ? 3000 : v_scroll_speed)) *
                             get_physics_process_delta_time();

                if (Math::abs(vel) >= dist) {
                    set_v_scroll(target_v_scroll);
                    scrolling = false;
                    minimap_clicked = false;
                    set_physics_process_internal(false);
                } else {
                    set_v_scroll(get_v_scroll() + vel);
                }
            } else {
                scrolling = false;
                minimap_clicked = false;
                set_physics_process_internal(false);
            }
        } break;
        case NOTIFICATION_DRAW: {

            if (first_draw) {
                // Size may not be the final one, so attempts to ensure cursor was visible may have failed.
                adjust_viewport_to_cursor();
                first_draw = false;
            }

            Size2 size = get_size();
            if ((!has_focus() && !menu->has_focus()) || !window_has_focus) {
                draw_caret = false;
            }

            if (draw_breakpoint_gutter || draw_bookmark_gutter) {
                breakpoint_gutter_width = (get_row_height() * 55) / 100;
                D()->cache.breakpoint_gutter_width = breakpoint_gutter_width;
            } else {
                D()->cache.breakpoint_gutter_width = 0;
            }

            if (draw_info_gutter) {
                info_gutter_width = (get_row_height());
                D()->cache.info_gutter_width = info_gutter_width;
            } else {
                D()->cache.info_gutter_width = 0;
            }

            if (draw_fold_gutter) {
                fold_gutter_width = (get_row_height() * 55) / 100;
                D()->cache.fold_gutter_width = fold_gutter_width;
            } else {
                D()->cache.fold_gutter_width = 0;
            }

            D()->cache.minimap_width = 0;
            if (draw_minimap) {
                D()->cache.minimap_width = minimap_width;
            }
            int line_number_char_count = 0;

            {
                int lc = D()->text.size();
                D()->cache.line_number_w = 0;
                while (lc) {
                    D()->cache.line_number_w += 1;
                    lc /= 10;
                }

                if (line_numbers) {

                    line_number_char_count = D()->cache.line_number_w;
                    D()->cache.line_number_w =
                            (D()->cache.line_number_w + 1) * D()->cache.font->get_char_size('0').width;
                } else {
                    D()->cache.line_number_w = 0;
                }
            }
            _update_scrollbars();

            RenderingEntity ci = get_canvas_item();
            RenderingServer::get_singleton()->canvas_item_set_clip(get_canvas_item(), true);
            int xmargin_beg = D()->cache.style_normal->get_margin(Margin::Left) + D()->cache.line_number_w +
                              D()->cache.breakpoint_gutter_width + D()->cache.fold_gutter_width +
                              D()->cache.info_gutter_width;
            int xmargin_end =
                    size.width - D()->cache.style_normal->get_margin(Margin::Right) - D()->cache.minimap_width;
            // Let's do it easy for now.
            D()->cache.style_normal->draw(ci, Rect2(Point2(), size));
            if (readonly) {
                D()->cache.style_readonly->draw(ci, Rect2(Point2(), size));
                draw_caret = false;
            }
            if (has_focus())
                D()->cache.style_focus->draw(ci, Rect2(Point2(), size));

            int ascent = D()->cache.font->get_ascent();

            int visible_rows = get_visible_rows() + 1;

            Color color = readonly ? D()->cache.font_color_readonly : D()->cache.font_color;

            if (syntax_coloring) {
                if (D()->cache.background_color.a > 0.01) {
                    RenderingServer::get_singleton()->canvas_item_add_rect(
                            ci, Rect2(Point2i(), get_size()), D()->cache.background_color);
                }
            }

            if (line_length_guidelines) {
                const int hard_x = xmargin_beg +
                                   (int)D()->cache.font->get_char_size('0').width * line_length_guideline_hard_col -
                                   D()->cursor.x_ofs;
                if (hard_x > xmargin_beg && hard_x < xmargin_end) {
                    RenderingServer::get_singleton()->canvas_item_add_line(
                            ci, Point2(hard_x, 0), Point2(hard_x, size.height), D()->cache.line_length_guideline_color);
                }

                // Draw a "Soft" line length guideline, less visible than the hard line length guideline.
                // It's usually set to a lower column compared to the hard line length guideline.
                // Only drawn if its column differs from the hard line length guideline.
                const int soft_x = xmargin_beg +
                                   (int)D()->cache.font->get_char_size('0').width * line_length_guideline_soft_col -
                                   D()->cursor.x_ofs;
                if (hard_x != soft_x && soft_x > xmargin_beg && soft_x < xmargin_end) {
                    RenderingServer::get_singleton()->canvas_item_add_line(ci, Point2(soft_x, 0),
                            Point2(soft_x, size.height), D()->cache.line_length_guideline_color * Color(1, 1, 1, 0.5));
                }
            }

            int brace_open_match_line = -1;
            int brace_open_match_column = -1;
            bool brace_open_matching = false;
            bool brace_open_mismatch = false;
            int brace_close_match_line = -1;
            int brace_close_match_column = -1;
            bool brace_close_matching = false;
            bool brace_close_mismatch = false;

            if (brace_matching_enabled && D()->cursor.line >= 0 && D()->cursor.line < D()->text.size() &&
                    D()->cursor.column >= 0) {

                if (D()->cursor.column < D()->text[D()->cursor.line].length()) {
                    // Check for open.
                    CharType c = D()->text[D()->cursor.line][D()->cursor.column];
                    CharType closec = 0;

                    if (c == '[') {
                        closec = ']';
                    } else if (c == '{') {
                        closec = '}';
                    } else if (c == '(') {
                        closec = ')';
                    }

                    if (closec != nullptr) {

                        int stack = 1;

                        for (int i = D()->cursor.line; i < D()->text.size(); i++) {

                            int from = i == D()->cursor.line ? D()->cursor.column + 1 : 0;
                            for (int j = from; j < D()->text[i].length(); j++) {

                                CharType cc = D()->text[i][j];
                                // Ignore any brackets inside a string.
                                if (cc == '"' || cc == '\'') {
                                    CharType quotation = cc;
                                    do {
                                        j++;
                                        if (!(j < D()->text[i].length())) {
                                            break;
                                        }
                                        cc = D()->text[i][j];
                                        // Skip over escaped quotation marks inside strings.
                                        if (cc == '\\') {
                                            bool escaped = true;
                                            while (j + 1 < D()->text[i].length() && D()->text[i][j + 1] == '\\') {
                                                escaped = !escaped;
                                                j++;
                                            }
                                            if (escaped) {
                                                j++;
                                                continue;
                                            }
                                        }
                                    } while (cc != quotation);
                                } else if (cc == c)
                                    stack++;
                                else if (cc == closec)
                                    stack--;

                                if (stack == 0) {
                                    brace_open_match_line = i;
                                    brace_open_match_column = j;
                                    brace_open_matching = true;

                                    break;
                                }
                            }
                            if (brace_open_match_line != -1)
                                break;
                        }

                        if (!brace_open_matching)
                            brace_open_mismatch = true;
                    }
                }

                if (D()->cursor.column > 0) {
                    CharType c = D()->text[D()->cursor.line][D()->cursor.column - 1];
                    CharType closec = 0;

                    if (c == ']') {
                        closec = '[';
                    } else if (c == '}') {
                        closec = '{';
                    } else if (c == ')') {
                        closec = '(';
                    }

                    if (closec != nullptr) {

                        int stack = 1;

                        for (int i = D()->cursor.line; i >= 0; i--) {

                            int from = i == D()->cursor.line ? D()->cursor.column - 2 : D()->text[i].length() - 1;
                            for (int j = from; j >= 0; j--) {

                                CharType cc = D()->text[i][j];
                                // Ignore any brackets inside a string.
                                if (cc == '"' || cc == '\'') {
                                    CharType quotation = cc;
                                    do {
                                        j--;
                                        if (!(j >= 0)) {
                                            break;
                                        }
                                        cc = D()->text[i][j];
                                        // Skip over escaped quotation marks inside strings.
                                        if (cc == quotation) {
                                            bool escaped = false;
                                            while (j - 1 >= 0 && D()->text[i][j - 1] == '\\') {
                                                escaped = !escaped;
                                                j--;
                                            }
                                            if (escaped) {
                                                cc = '\\';
                                                continue;
                                            }
                                        }
                                    } while (cc != quotation);
                                } else if (cc == c)
                                    stack++;
                                else if (cc == closec)
                                    stack--;

                                if (stack == 0) {
                                    brace_close_match_line = i;
                                    brace_close_match_column = j;
                                    brace_close_matching = true;

                                    break;
                                }
                            }
                            if (brace_close_match_line != -1)
                                break;
                        }

                        if (!brace_close_matching)
                            brace_close_mismatch = true;
                    }
                }
            }

            Point2 cursor_pos;
            bool is_cursor_visible = false;
            int cursor_insert_offset_y = 0;

            // Get the highlighted words.
            UIString highlighted_text = StringUtils::from_utf8(get_selection_text());

            // Check if highlighted words contains only whitespaces (tabs or spaces).
            bool only_whitespaces_highlighted =StringUtils::strip_edges( highlighted_text).isEmpty();

            UIString line_num_padding(line_numbers_zero_padded ? "0" : " ");

            int cursor_wrap_index = get_cursor_wrap_index();

            FontDrawer drawer(D()->cache.font, Color(1, 1, 1));

            int first_visible_line = get_first_visible_line() - 1;
            int draw_amount = visible_rows + (smooth_scroll_enabled ? 1 : 0);
            draw_amount += get_line_wrap_count(first_visible_line + 1);

            // Draw minimap.
            if (draw_minimap) {
                int minimap_visible_lines = _get_minimap_visible_rows();
                int minimap_line_height = (minimap_char_size.y + minimap_line_spacing);
                int minimap_tab_size = minimap_char_size.x * indent_size;

                // calculate viewport size and y offset
                int viewport_height = (draw_amount - 1) * minimap_line_height;
                int control_height = _get_control_height() - viewport_height;
                int viewport_offset_y =
                        round(get_scroll_pos_for_line(first_visible_line + 1) * control_height) /
                        ((v_scroll->get_max() <= minimap_visible_lines) ? (minimap_visible_lines - draw_amount) :
                                                                          (v_scroll->get_max() - draw_amount));

                // calculate the first line.
                int num_lines_before = round((viewport_offset_y) / minimap_line_height);
                int wi;
                int minimap_line = (v_scroll->get_max() <= minimap_visible_lines) ? -1 : first_visible_line;
                if (minimap_line >= 0) {
                    minimap_line -= num_lines_from_rows(first_visible_line, 0, -num_lines_before, wi);
                    minimap_line -= (minimap_line > 0 && smooth_scroll_enabled ? 1 : 0);
                }
                int minimap_draw_amount = minimap_visible_lines + get_line_wrap_count(minimap_line + 1);

                // Draw the minimap.

                // Add visual feedback when dragging or hovering the the visible area rectangle.
                float viewport_alpha;
                if (dragging_minimap) {
                    viewport_alpha = 0.25;
                } else if (D()->hovering_minimap) {
                    viewport_alpha = 0.175;
                } else {
                    viewport_alpha = 0.1;
                }

                const Color viewport_color = (D()->cache.background_color.get_v() < 0.5) ?
                                                     Color(1, 1, 1, viewport_alpha) :
                                                     Color(0, 0, 0, viewport_alpha);
                RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                        Rect2((xmargin_end + 2), viewport_offset_y, D()->cache.minimap_width, viewport_height),
                        viewport_color);
                for (int i = 0; i < minimap_draw_amount; i++) {

                    minimap_line++;

                    if (minimap_line < 0 || minimap_line >= (int)D()->text.size()) {
                        break;
                    }

                    while (is_line_hidden(minimap_line)) {
                        minimap_line++;
                        if (minimap_line < 0 || minimap_line >= (int)D()->text.size()) {
                            break;
                        }
                    }
                    if (minimap_line < 0 || minimap_line >= (int)D()->text.size()) {
                        break;
                    }
                    Map<int, HighlighterInfo> color_map;
                    if (syntax_coloring) {
                        color_map = D()->_get_line_syntax_highlighting(this,minimap_line);
                    }

                    Color current_color = D()->cache.font_color;
                    if (readonly) {
                        current_color = D()->cache.font_color_readonly;
                    }

                    Vector<UIString> wrap_rows = get_wrap_rows_text(minimap_line);
                    int line_wrap_amount = get_line_wrap_count(minimap_line);
                    int last_wrap_column = 0;

                    for (int line_wrap_index = 0; line_wrap_index < line_wrap_amount + 1; line_wrap_index++) {
                        if (line_wrap_index != 0) {
                            i++;
                            if (i >= minimap_draw_amount)
                                break;
                        }

                        const UIString &str = wrap_rows[line_wrap_index];
                        int indent_px = line_wrap_index != 0 ? get_indent_level(minimap_line) : 0;
                        if (indent_px >= D()->wrap_at) {
                            indent_px = 0;
                        }
                        indent_px = minimap_char_size.x * indent_px;

                        if (line_wrap_index > 0) {
                            last_wrap_column += wrap_rows[line_wrap_index - 1].length();
                        }

                        if (minimap_line == D()->cursor.line && cursor_wrap_index == line_wrap_index &&
                                highlight_current_line) {
                            RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                    Rect2((xmargin_end + 2), i * 3, D()->cache.minimap_width, 2),
                                    D()->cache.current_line_color);
                        }

                        Color previous_color;
                        int characters = 0;
                        int tabs = 0;
                        for (int j = 0; j < str.length(); j++) {
                            if (syntax_coloring) {
                                if (color_map.contains(last_wrap_column + j)) {
                                    current_color = color_map[last_wrap_column + j].color;
                                    if (readonly) {
                                        current_color.a = D()->cache.font_color_readonly.a;
                                    }
                                }
                                color = current_color;
                            }

                            if (j == 0) {
                                previous_color = color;
                            }

                            int xpos = indent_px + ((xmargin_end + minimap_char_size.x) + (minimap_char_size.x * j)) +
                                       tabs;
                            bool out_of_bounds = (xpos >= xmargin_end + D()->cache.minimap_width);

                            bool is_whitespace = _is_whitespace(str[j]);
                            if (!is_whitespace) {
                                characters++;

                                if (j < str.length() - 1 && color == previous_color && !out_of_bounds) {
                                    continue;
                                }

                                // If we've changed colour we are at the start of a new section, therefore we need to go
                                // back to the end of the previous section to draw it, we'll also add the character back
                                // on.
                                if (color != previous_color) {
                                    characters--;
                                    j--;

                                    if (str[j] == '\t') {
                                        tabs -= minimap_tab_size;
                                    }
                                }
                            }

                            if (characters > 0) {
                                previous_color.a *= 0.6f;
                                // take one for zero indexing, and if we hit whitespace / the end of a word.
                                int chars = M_MAX(0, (j - (characters - 1)) - (is_whitespace ? 1 : 0)) + 1;
                                int char_x_ofs = indent_px +
                                                 ((xmargin_end + minimap_char_size.x) + (minimap_char_size.x * chars)) +
                                                 tabs;
                                RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                        Rect2(Point2(char_x_ofs, minimap_line_height * i),
                                                Point2(minimap_char_size.x * characters, minimap_char_size.y)),
                                        previous_color);
                            }

                            if (out_of_bounds) {
                                break;
                            }

                            // re-adjust if we went backwards.
                            if (color != previous_color && !is_whitespace) {
                                characters++;
                            }

                            if (str[j] == '\t') {
                                tabs += minimap_tab_size;
                            }

                            previous_color = color;
                            characters = 0;
                        }
                    }
                }
            }

            int top_limit_y = 0;
            int bottom_limit_y = get_size().height;
            if (readonly) {
                top_limit_y += D()->cache.style_readonly->get_margin(Margin::Bottom);
                bottom_limit_y -= D()->cache.style_readonly->get_margin(Margin::Bottom);
            } else {
                top_limit_y += D()->cache.style_normal->get_margin(Margin::Top);
                bottom_limit_y -= D()->cache.style_normal->get_margin(Margin::Top);
            }
            // Draw main text.
            D()->line_drawing_cache.clear();
            int line = first_visible_line;
            for (int i = 0; i < draw_amount; i++) {

                line++;

                if (line < 0 || line >= (int)D()->text.size())
                    continue;

                while (is_line_hidden(line)) {
                    line++;
                    if (line < 0 || line >= (int)D()->text.size()) {
                        break;
                    }
                }

                if (line < 0 || line >= (int)D()->text.size())
                    continue;

                const UIString &fullstr = D()->text[line];
                PrivateData::LineDrawingCache cache_entry;

                Map<int, HighlighterInfo> color_map;
                if (syntax_coloring) {
                    color_map = D()->_get_line_syntax_highlighting(this,line);
                }
                // Ensure we at least use the font color.
                Color current_color = readonly ? D()->cache.font_color_readonly : D()->cache.font_color;

                bool underlined = false;

                Vector<UIString> wrap_rows = get_wrap_rows_text(line);
                int line_wrap_amount = get_line_wrap_count(line);
                int last_wrap_column = 0;
                int wrap_column_offset = 0;

                for (int line_wrap_index = 0; line_wrap_index < line_wrap_amount + 1; line_wrap_index++) {
                    if (line_wrap_index != 0) {
                        i++;
                        if (i >= draw_amount)
                            break;
                    }

                    const UIString &str = wrap_rows[line_wrap_index];
                    int indent_px = line_wrap_index != 0 ?
                                            get_indent_level(line) * D()->cache.font->get_char_size(' ').width :
                                            0;
                    if (indent_px >= D()->wrap_at) {
                        indent_px = 0;
                    }

                    if (line_wrap_index > 0)
                        last_wrap_column += wrap_rows[line_wrap_index - 1].length();

                    int char_margin = xmargin_beg - D()->cursor.x_ofs;
                    char_margin += indent_px;
                    int char_ofs = 0;

                    int ofs_x = 0;
                    int ofs_y = 0;
                    if (readonly) {
                        ofs_x = D()->cache.style_readonly->get_offset().x / 2;
                        ofs_x -= D()->cache.style_normal->get_offset().x / 2;
                        ofs_y = D()->cache.style_readonly->get_offset().y / 2;
                    } else {
                        ofs_y = D()->cache.style_normal->get_offset().y / 2;
                    }

                    ofs_y += (i * get_row_height() + D()->cache.line_spacing / 2);
                    ofs_y -= D()->cursor.wrap_ofs * get_row_height();
                    ofs_y -= get_v_scroll_offset() * get_row_height();
                    bool clipped = false;
                    if (ofs_y + get_row_height() < top_limit_y) {
                        // Line is outside the top margin, clip current line.
                        // Still need to go through the process to prepare color changes for next lines.
                        clipped = true;
                    }

                    if (ofs_y > bottom_limit_y) {
                        // Line is outside the bottom margin, clip any remaining text.
                        i = draw_amount;
                        break;
                    }

                    // Check if line contains highlighted word.
                    int highlighted_text_col = -1;
                    int search_text_col = -1;
                    int highlighted_word_col = -1;

                    if (!D()->search_text.isEmpty())
                        search_text_col = D()->_get_column_pos_of_word(D()->search_text, str, D()->search_flags, 0);

                    if (highlighted_text.length() != 0 && highlighted_text != D()->search_text)
                        highlighted_text_col = D()->_get_column_pos_of_word(
                                highlighted_text, str, SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, 0);

                    if (select_identifiers_enabled && D()->highlighted_word.length() != 0) {
                        if (_is_char(D()->highlighted_word[0]) || D()->highlighted_word[0] == '.') {
                            highlighted_word_col = D()->_get_column_pos_of_word(
                                    D()->highlighted_word, fullstr, SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, 0);
                        }
                    }

                    if (D()->text.is_marked(line)) {
                        RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                Rect2(xmargin_beg + ofs_x, ofs_y, xmargin_end - xmargin_beg, get_row_height()),
                                D()->cache.mark_color);
                    }

                    if (str.length() == 0) {
                        // Draw line background if empty as we won't loop at at all.
                        if (line == D()->cursor.line && cursor_wrap_index == line_wrap_index &&
                                highlight_current_line) {
                            RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                    Rect2(ofs_x, ofs_y, xmargin_end, get_row_height()), D()->cache.current_line_color);
                        }

                        // Give visual indication of empty selected line.
                        if (D()->selection.active && line >= D()->selection.from_line &&
                                line <= D()->selection.to_line && char_margin >= xmargin_beg) {
                            int char_w = D()->cache.font->get_char_size(' ').width;
                            RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                    Rect2(xmargin_beg + ofs_x, ofs_y, char_w, get_row_height()),
                                    D()->cache.selection_color);
                        }
                    } else {
                        // If it has text, then draw current line marker in the margin, as line number etc will draw
                        // over it, draw the rest of line marker later.
                        if (line == D()->cursor.line && cursor_wrap_index == line_wrap_index &&
                                highlight_current_line) {
                            RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                    Rect2(0, ofs_y, xmargin_beg + ofs_x, get_row_height()),
                                    D()->cache.current_line_color);
                        }
                    }

                    if (line_wrap_index == 0) {
                        // Only do these if we are on the first wrapped part of a line.

                        cache_entry.y_offset = ofs_y;
                        if (D()->text.is_breakpoint(line) && !draw_breakpoint_gutter) {
#ifdef TOOLS_ENABLED
                            RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                    Rect2(xmargin_beg + ofs_x, ofs_y + get_row_height() - EDSCALE,
                                            xmargin_end - xmargin_beg, EDSCALE),
                                    D()->cache.breakpoint_color);
#else
                            RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                    Rect2(xmargin_beg + ofs_x, ofs_y, xmargin_end - xmargin_beg, get_row_height()),
                                    D()->cache.breakpoint_color);
#endif
                        }

                        // Draw bookmark marker.
                        if (D()->text.is_bookmark(line)) {
                            if (draw_bookmark_gutter) {
                                int vertical_gap = (get_row_height() * 40) / 100;
                                int horizontal_gap = (D()->cache.breakpoint_gutter_width * 30) / 100;
                                int marker_radius = get_row_height() - (vertical_gap * 2);
                                RenderingServer::get_singleton()->canvas_item_add_circle(ci,
                                        Point2(D()->cache.style_normal->get_margin(Margin::Left) + horizontal_gap - 2 +
                                                        marker_radius / 2,
                                                ofs_y + vertical_gap + marker_radius / 2),
                                        marker_radius,
                                        Color(D()->cache.bookmark_color.r, D()->cache.bookmark_color.g,
                                                D()->cache.bookmark_color.b));
                            }
                        }

                        // Draw breakpoint marker.
                        if (D()->text.is_breakpoint(line)) {
                            if (draw_breakpoint_gutter) {
                                int vertical_gap = (get_row_height() * 40) / 100;
                                int horizontal_gap = (D()->cache.breakpoint_gutter_width * 30) / 100;
                                int marker_height = get_row_height() - (vertical_gap * 2);
                                int marker_width = D()->cache.breakpoint_gutter_width - (horizontal_gap * 2);
                                // No transparency on marker.
                                RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                        Rect2(D()->cache.style_normal->get_margin(Margin::Left) + horizontal_gap - 2,
                                                ofs_y + vertical_gap, marker_width, marker_height),
                                        Color(D()->cache.breakpoint_color.r, D()->cache.breakpoint_color.g,
                                                D()->cache.breakpoint_color.b));
                            }
                        }

                        // Draw info icons.
                        if (draw_info_gutter && D()->text.has_info_icon(line)) {
                            int vertical_gap = (get_row_height() * 40) / 100;
                            int horizontal_gap = (D()->cache.info_gutter_width * 30) / 100;
                            int gutter_left = D()->cache.style_normal->get_margin(Margin::Left) +
                                              D()->cache.breakpoint_gutter_width;

                            Ref<Texture> info_icon = D()->text.get_info_icon(line);
                            // Ensure the icon fits the gutter size.
                            Size2i icon_size = info_icon->get_size();
                            if (icon_size.width > D()->cache.info_gutter_width - horizontal_gap) {
                                icon_size.width = D()->cache.info_gutter_width - horizontal_gap;
                            }
                            if (icon_size.height > get_row_height() - horizontal_gap) {
                                icon_size.height = get_row_height() - horizontal_gap;
                            }

                            Size2i icon_pos;
                            int xofs = horizontal_gap - (info_icon->get_width() / 4);
                            int yofs = vertical_gap - (info_icon->get_height() / 4);
                            icon_pos.x = gutter_left + xofs + ofs_x;
                            icon_pos.y = ofs_y + yofs;

                            draw_texture_rect(info_icon, Rect2(icon_pos, icon_size));
                        }

                        // Draw execution marker.
                        if (executing_line == line) {
                            if (draw_breakpoint_gutter) {
                                int icon_extra_size = 4;
                                int vertical_gap = (get_row_height() * 40) / 100;
                                int horizontal_gap = (D()->cache.breakpoint_gutter_width * 30) / 100;
                                int marker_height = get_row_height() - (vertical_gap * 2) + icon_extra_size;
                                int marker_width =
                                        D()->cache.breakpoint_gutter_width - (horizontal_gap * 2) + icon_extra_size;
                                D()->cache.executing_icon->draw_rect(ci,
                                        Rect2(D()->cache.style_normal->get_margin(Margin::Left) + horizontal_gap - 2 -
                                                        icon_extra_size / 2,
                                                ofs_y + vertical_gap - icon_extra_size / 2, marker_width,
                                                marker_height),
                                        false,
                                        Color(D()->cache.executing_line_color.r, D()->cache.executing_line_color.g,
                                                D()->cache.executing_line_color.b));
                            } else {
#ifdef TOOLS_ENABLED
                                RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                        Rect2(xmargin_beg + ofs_x, ofs_y + get_row_height() - EDSCALE,
                                                xmargin_end - xmargin_beg, EDSCALE),
                                        D()->cache.executing_line_color);
#else
                                RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                        Rect2(xmargin_beg + ofs_x, ofs_y, xmargin_end - xmargin_beg, get_row_height()),
                                        D()->cache.executing_line_color);
#endif
                            }
                        }

                        // Draw fold markers.
                        if (draw_fold_gutter) {
                            int horizontal_gap = (D()->cache.fold_gutter_width * 30) / 100;
                            int gutter_left = D()->cache.style_normal->get_margin(Margin::Left) +
                                              D()->cache.breakpoint_gutter_width + D()->cache.line_number_w +
                                              D()->cache.info_gutter_width;
                            if (is_folded(line)) {
                                int xofs = horizontal_gap - (D()->cache.can_fold_icon->get_width()) / 2;
                                int yofs = (get_row_height() - D()->cache.folded_icon->get_height()) / 2;
                                D()->cache.folded_icon->draw(ci, Point2(gutter_left + xofs + ofs_x, ofs_y + yofs),
                                        D()->cache.code_folding_color);
                            } else if (can_fold(line)) {
                                int xofs = -D()->cache.can_fold_icon->get_width() / 2 - horizontal_gap + 3;
                                int yofs = (get_row_height() - D()->cache.can_fold_icon->get_height()) / 2;
                                D()->cache.can_fold_icon->draw(ci, Point2(gutter_left + xofs + ofs_x, ofs_y + yofs),
                                        D()->cache.code_folding_color);
                            }
                        }

                        // Draw line numbers.
                        if (D()->cache.line_number_w) {
                            int yofs = ofs_y + (get_row_height() - D()->cache.font->get_height()) / 2;
                            UIString fc(UIString::number(line + 1));
                            while (fc.length() < line_number_char_count) {
                                fc = line_num_padding + fc;
                            }

                            D()->cache.font->draw_ui_string(ci,
                                    Point2(D()->cache.style_normal->get_margin(Margin::Left) +
                                                    D()->cache.breakpoint_gutter_width + D()->cache.info_gutter_width +
                                                    ofs_x,
                                            yofs + D()->cache.font->get_ascent()),
                                    fc,
                                    D()->text.is_safe(line) ? D()->cache.safe_line_number_color :
                                                              D()->cache.line_number_color);
                        }
                    }

                    int first_visible_char = str.length();
                    int last_visible_char = 0;
                    // Loop through characters in one line.
                    int j = 0;
                    for ( ; j < str.length(); j++) {
                        CharType next_c = (j+1)<str.length() ? str[j + 1] : CharType(0);

                        if (syntax_coloring) {
                            if (color_map.contains(last_wrap_column + j)) {
                                current_color = color_map[last_wrap_column + j].color;
                                if (readonly && current_color.a > D()->cache.font_color_readonly.a) {
                                    current_color.a = D()->cache.font_color_readonly.a;
                                }
                            }
                            color = current_color;
                        }

                        int char_w;

                        // Handle tabulator.
                        char_w = D()->text.get_char_width(str[j], next_c, char_ofs);

                        if ((char_ofs + char_margin) < xmargin_beg) {
                            char_ofs += char_w;

                            // Line highlighting handle horizontal clipping.
                            if (line == D()->cursor.line && cursor_wrap_index == line_wrap_index &&
                                    highlight_current_line) {

                                if (j == str.length() - 1) {
                                    // End of line when last char is skipped.
                                    RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                            Rect2(xmargin_beg + ofs_x, ofs_y,
                                                    xmargin_end - (char_ofs + char_margin + char_w), get_row_height()),
                                            D()->cache.current_line_color);
                                } else if ((char_ofs + char_margin) > xmargin_beg) {
                                    // Char next to margin is skipped.
                                    RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                            Rect2(xmargin_beg + ofs_x, ofs_y,
                                                    (char_ofs + char_margin) - (xmargin_beg + ofs_x), get_row_height()),
                                            D()->cache.current_line_color);
                                }
                            }
                            continue;
                        }

                        if ((char_ofs + char_margin + char_w) >= xmargin_end) {
                            break;
                        }

                        bool in_search_result = false;

                        if (search_text_col != -1) {
                            // If we are at the end check for new search result on same line.
                            if (j >= search_text_col + D()->search_text.length())
                                search_text_col =
                                        D()->_get_column_pos_of_word(D()->search_text, str, D()->search_flags, j);

                            in_search_result = j >= search_text_col && j < search_text_col + D()->search_text.length();

                            if (in_search_result) {
                                RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                        Rect2(Point2i(char_ofs + char_margin, ofs_y), Size2i(char_w, get_row_height())),
                                        D()->cache.search_result_color);
                            }
                        }

                        // Current line highlighting.
                        bool in_selection = (D()->selection.active && line >= D()->selection.from_line &&
                                             line <= D()->selection.to_line &&
                                             (line > D()->selection.from_line ||
                                                     last_wrap_column + j >= D()->selection.from_column) &&
                                             (line < D()->selection.to_line ||
                                                     last_wrap_column + j < D()->selection.to_column));

                        if (!clipped && line == D()->cursor.line && cursor_wrap_index == line_wrap_index &&
                                highlight_current_line) {
                            // Draw the wrap indent offset highlight.
                            if (line_wrap_index != 0 && j == 0) {
                                RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                        Rect2(char_ofs + char_margin + ofs_x - indent_px, ofs_y, indent_px,
                                                get_row_height()),
                                        D()->cache.current_line_color);
                            }
                            // If its the last char draw to end of the line.
                            if (j == str.length() - 1) {
                                RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                        Rect2(char_ofs + char_margin + char_w + ofs_x, ofs_y,
                                                xmargin_end - (char_ofs + char_margin + char_w), get_row_height()),
                                        D()->cache.current_line_color);
                            }
                            // Actual text.
                            if (!in_selection) {
                                RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                        Rect2(Point2i(char_ofs + char_margin + ofs_x, ofs_y),
                                                Size2i(char_w, get_row_height())),
                                        D()->cache.current_line_color);
                            }
                        }

                        if (!clipped && in_selection) {
                            RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                    Rect2(Point2i(char_ofs + char_margin + ofs_x, ofs_y),
                                            Size2i(char_w, get_row_height())),
                                    D()->cache.selection_color);
                        }

                        if (!clipped && in_search_result) {
                            Color border_color = (line == search_result_line && j >= search_result_col &&
                                                         j < search_result_col + D()->search_text.length()) ?
                                                         D()->cache.font_color :
                                                         D()->cache.search_result_border_color;

                            RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                    Rect2(Point2i(char_ofs + char_margin + ofs_x, ofs_y), Size2i(char_w, 1)),
                                    border_color);
                            RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                    Rect2(Point2i(char_ofs + char_margin + ofs_x, ofs_y + get_row_height() - 1),
                                            Size2i(char_w, 1)),
                                    border_color);

                            if (j == search_text_col)
                                RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                        Rect2(Point2i(char_ofs + char_margin + ofs_x, ofs_y),
                                                Size2i(1, get_row_height())),
                                        border_color);
                            if (j == search_text_col + D()->search_text.length() - 1)
                                RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                        Rect2(Point2i(char_ofs + char_margin + char_w + ofs_x - 1, ofs_y),
                                                Size2i(1, get_row_height())),
                                        border_color);
                        }

                        if (!clipped && highlight_all_occurrences && !only_whitespaces_highlighted) {
                            if (highlighted_text_col != -1) {

                                // If we are at the end check for new word on same line.
                                if (j > highlighted_text_col + highlighted_text.length()) {
                                    highlighted_text_col = D()->_get_column_pos_of_word(
                                            highlighted_text, str, SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, j);
                                }

                                bool in_highlighted_word = (j >= highlighted_text_col &&
                                                            j < highlighted_text_col + highlighted_text.length());

                                // If this is the original highlighted text we don't want to highlight it again.
                                if (D()->cursor.line == line && cursor_wrap_index == line_wrap_index &&
                                        (D()->cursor.column >= highlighted_text_col &&
                                                D()->cursor.column <=
                                                        highlighted_text_col + highlighted_text.length())) {
                                    in_highlighted_word = false;
                                }

                                if (in_highlighted_word) {
                                    RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                            Rect2(Point2i(char_ofs + char_margin + ofs_x, ofs_y),
                                                    Size2i(char_w, get_row_height())),
                                            D()->cache.word_highlighted_color);
                                }
                            }
                        }

                        if (highlighted_word_col != -1) {
                            if (j + last_wrap_column > highlighted_word_col + D()->highlighted_word.length()) {
                                highlighted_word_col = D()->_get_column_pos_of_word(D()->highlighted_word, fullstr,
                                        SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, j + last_wrap_column);
                            }
                            underlined = (j + last_wrap_column >= highlighted_word_col &&
                                          j + last_wrap_column < highlighted_word_col + D()->highlighted_word.length());
                        }

                        if (brace_matching_enabled) {
                            int yofs = ofs_y + (get_row_height() - D()->cache.font->get_height()) / 2;
                            if ((brace_open_match_line == line && brace_open_match_column == last_wrap_column + j) ||
                                    (D()->cursor.column == last_wrap_column + j && D()->cursor.line == line &&
                                            cursor_wrap_index == line_wrap_index &&
                                            (brace_open_matching || brace_open_mismatch))) {

                                if (brace_open_mismatch)
                                    color = D()->cache.brace_mismatch_color;
                                drawer.draw_char(ci, Point2i(char_ofs + char_margin + ofs_x, yofs + ascent), '_',
                                        next_c,
                                        in_selection && override_selected_font_color ? D()->cache.font_color_selected :
                                                                                       color);
                            }

                            if ((brace_close_match_line == line && brace_close_match_column == last_wrap_column + j) ||
                                    (D()->cursor.column == last_wrap_column + j + 1 && D()->cursor.line == line &&
                                            cursor_wrap_index == line_wrap_index &&
                                            (brace_close_matching || brace_close_mismatch))) {

                                if (brace_close_mismatch)
                                    color = D()->cache.brace_mismatch_color;
                                drawer.draw_char(ci, Point2i(char_ofs + char_margin + ofs_x, yofs + ascent), '_',
                                        next_c,
                                        in_selection && override_selected_font_color ? D()->cache.font_color_selected :
                                                                                       color);
                            }
                        }

                        if (!clipped && D()->cursor.column == last_wrap_column + j && D()->cursor.line == line &&
                                cursor_wrap_index == line_wrap_index) {
                            is_cursor_visible = true;

                            cursor_pos = Point2i(char_ofs + char_margin + ofs_x, ofs_y);
                            cursor_pos.y += (get_row_height() - D()->cache.font->get_height()) / 2;

                            if (insert_mode) {
                                cursor_insert_offset_y = (D()->cache.font->get_height() - 3);
                                cursor_pos.y += cursor_insert_offset_y;
                            }

                            int caret_w = (str[j] == '\t') ? D()->cache.font->get_char_size(' ').width : char_w;
                            if (D()->ime_text.length() > 0) {
                                int ofs = 0;
                                while (true) {
                                    if (ofs >= D()->ime_text.length())
                                        break;

                                    CharType cchar = D()->ime_text[ofs];
                                    CharType next = D()->ime_text[ofs + 1];
                                    int im_char_width = D()->cache.font->get_char_size(cchar, next).width;

                                    if ((char_ofs + char_margin + im_char_width) >= xmargin_end)
                                        break;

                                    bool selected = ofs >= D()->ime_selection.x &&
                                                    ofs < D()->ime_selection.x + D()->ime_selection.y;
                                    if (selected) {
                                        RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                                Rect2(Point2(char_ofs + char_margin, ofs_y + get_row_height()),
                                                        Size2(im_char_width, 3)),
                                                color);
                                    } else {
                                        RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                                Rect2(Point2(char_ofs + char_margin, ofs_y + get_row_height()),
                                                        Size2(im_char_width, 1)),
                                                color);
                                    }

                                    drawer.draw_char(ci, Point2(char_ofs + char_margin + ofs_x, ofs_y + ascent), cchar,
                                            next, color);

                                    char_ofs += im_char_width;
                                    ofs++;
                                }
                            }
                            if (D()->ime_text.length() == 0) {
                                if (draw_caret || drag_caret_force_displayed) {
                                    if (insert_mode) {
#ifdef TOOLS_ENABLED
                                        int caret_h = (block_caret) ? 4 : 2 * EDSCALE;
#else
                                        int caret_h = (block_caret) ? 4 : 2;
#endif
                                        RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                                Rect2(cursor_pos, Size2i(caret_w, caret_h)), D()->cache.caret_color);
                                    } else {
#ifdef TOOLS_ENABLED
                                        caret_w = (block_caret) ? caret_w : 2 * EDSCALE;
#else
                                        caret_w = (block_caret) ? caret_w : 2;
#endif

                                        RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                                Rect2(cursor_pos, Size2i(caret_w, D()->cache.font->get_height())),
                                                D()->cache.caret_color);
                                    }
                                }
                            }
                        }

                        if (!clipped) {
                            if (D()->cursor.column == last_wrap_column + j && D()->cursor.line == line &&
                                    cursor_wrap_index == line_wrap_index && block_caret && draw_caret && !insert_mode) {
                            color = D()->cache.caret_background_color;
                        } else if (!syntax_coloring && block_caret) {
                            color = readonly ? D()->cache.font_color_readonly : D()->cache.font_color;
                        }

                        if (str[j] >= 32) {
                            int yofs = ofs_y + (get_row_height() - D()->cache.font->get_height()) / 2;
                                int w = drawer.draw_char(ci, Point2i(char_ofs + char_margin + ofs_x, yofs + ascent),
                                        str[j], next_c,
                                        in_selection && override_selected_font_color ? D()->cache.font_color_selected :
                                                                                       color);
                            if (underlined) {
                                float line_width = 1.0;
#ifdef TOOLS_ENABLED
                                line_width *= EDSCALE;
#endif

                                    draw_rect_filled(
                                            Rect2(char_ofs + char_margin + ofs_x, yofs + ascent + 2, w, line_width),
                                            in_selection && override_selected_font_color ?
                                                    D()->cache.font_color_selected :
                                                    color);
                            }
                        } else if (draw_tabs && str[j] == '\t') {
                            int yofs = (get_row_height() - D()->cache.tab_icon->get_height()) / 2;
                                D()->cache.tab_icon->draw(ci, Point2(char_ofs + char_margin + ofs_x, ofs_y + yofs),
                                        in_selection && override_selected_font_color ? D()->cache.font_color_selected :
                                                                                       color);
                        }

                        if (draw_spaces && str[j] == ' ') {
                            int yofs = (get_row_height() - D()->cache.space_icon->get_height()) / 2;
                                D()->cache.space_icon->draw(ci, Point2(char_ofs + char_margin + ofs_x, ofs_y + yofs),
                                        in_selection && override_selected_font_color ? D()->cache.font_color_selected :
                                                                                       color);
                        }

                            if (first_visible_char > j) {
                                first_visible_char = j;
                            }
                            if (last_visible_char < j) {
                                last_visible_char = j;
                            }
                        }
                        char_ofs += char_w;

                        if (line_wrap_index == line_wrap_amount && j == str.length() - 1 && is_folded(line)) {
                            int yofs = (get_row_height() - D()->cache.folded_eol_icon->get_height()) / 2;
                            int xofs = D()->cache.folded_eol_icon->get_width() / 2;
                            Color eol_color = D()->cache.code_folding_color;
                            eol_color.a = 1;
                            D()->cache.folded_eol_icon->draw(
                                    ci, Point2(char_ofs + char_margin + xofs + ofs_x, ofs_y + yofs), eol_color);
                        }
                    }

                    if (!clipped && D()->cursor.column == (last_wrap_column + j) && D()->cursor.line == line &&
                            cursor_wrap_index == line_wrap_index && (char_ofs + char_margin) >= xmargin_beg) {

                        is_cursor_visible = true;
                        cursor_pos = Point2i(char_ofs + char_margin + ofs_x, ofs_y);
                        cursor_pos.y += (get_row_height() - D()->cache.font->get_height()) / 2;

                        if (insert_mode) {
                            cursor_insert_offset_y = D()->cache.font->get_height() - 3;
                            cursor_pos.y += cursor_insert_offset_y;
                        }
                        if (D()->ime_text.length() > 0) {
                            int ofs = 0;
                            while (true) {
                                if (ofs >= D()->ime_text.length())
                                    break;

                                CharType cchar = D()->ime_text[ofs];
                                CharType next = D()->ime_text[ofs + 1];
                                int im_char_width = D()->cache.font->get_char_size(cchar, next).width;

                                if ((char_ofs + char_margin + im_char_width) >= xmargin_end)
                                    break;

                                bool selected = ofs >= D()->ime_selection.x &&
                                                ofs < D()->ime_selection.x + D()->ime_selection.y;
                                if (selected) {
                                    RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                            Rect2(Point2(char_ofs + char_margin, ofs_y + get_row_height()),
                                                    Size2(im_char_width, 3)),
                                            color);
                                } else {
                                    RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                            Rect2(Point2(char_ofs + char_margin, ofs_y + get_row_height()),
                                                    Size2(im_char_width, 1)),
                                            color);
                                }

                                drawer.draw_char(
                                        ci, Point2(char_ofs + char_margin + ofs_x, ofs_y + ascent), cchar, next, color);

                                char_ofs += im_char_width;
                                ofs++;
                            }
                        }
                        if (D()->ime_text.isEmpty()) {
                            if (draw_caret || drag_caret_force_displayed) {
                                if (insert_mode) {
                                    int char_w = D()->cache.font->get_char_size(' ').width;
#ifdef TOOLS_ENABLED
                                    int caret_h = (block_caret) ? 4 : 2 * EDSCALE;
#else
                                    int caret_h = (block_caret) ? 4 : 2;
#endif
                                    RenderingServer::get_singleton()->canvas_item_add_rect(
                                            ci, Rect2(cursor_pos, Size2i(char_w, caret_h)), D()->cache.caret_color);
                                } else {
                                    int char_w = D()->cache.font->get_char_size(' ').width;
#ifdef TOOLS_ENABLED
                                    int caret_w = (block_caret) ? char_w : 2 * EDSCALE;
#else
                                    int caret_w = (block_caret) ? char_w : 2;
#endif

                                    RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                                            Rect2(cursor_pos, Size2i(caret_w, D()->cache.font->get_height())),
                                            D()->cache.caret_color);
                                }
                            }
                        }
                    }
                    cache_entry.first_visible_char.push_back(wrap_column_offset + first_visible_char);
                    cache_entry.last_visible_char.push_back(wrap_column_offset + last_visible_char);

                    wrap_column_offset += str.length();
                }
                D()->line_drawing_cache[line] = eastl::move(cache_entry);
            }

            bool completion_below = false;
            if (D()->completion_active && is_cursor_visible && D()->completion_options.size() > 0) {
                // Completion panel

                const Ref<StyleBox> csb = get_theme_stylebox("completion");
                const int maxlines = get_theme_constant("completion_lines");
                const int cmax_width =
                        get_theme_constant("completion_max_width") * D()->cache.font->get_char_size('x').x;
                const Color scrollc = get_theme_color("completion_scroll_color");

                const int row_height = get_row_height();
                const int completion_options_size = D()->completion_options.size();
                const int row_count = MIN(completion_options_size, maxlines);
                const int completion_rows_height = row_count * row_height;
                const int completion_base_width = D()->cache.font->get_string_size(D()->completion_base).width;

                int scroll_rectangle_width = get_theme_constant("completion_scroll_width");
                int width = 0;

                // Compute max width of the panel based on the longest completion option.
                // Limit the number of results for automatic width calculation to avoid freezing while showing results.
                if (completion_options_size < 1000) {
                    for (int i = 0; i < completion_options_size; i++) {
                        int line_width =
                                MIN(D()->cache.font->get_string_size(D()->completion_options[i].display).x, cmax_width);
                        if (line_width > width) {
                            width = line_width;
                        }
                    }
                } else {
                    // Fall back to predetermined width.
                    width = cmax_width;
                }

                // Add space for completion icons.
                const int icon_hsep = get_theme_constant("hseparation", "ItemList");
                const Size2 icon_area_size(row_height, row_height);
                const int icon_area_width = icon_area_size.width + icon_hsep;
                width += icon_area_size.width + icon_hsep;

                const int line_from =
                        CLAMP(D()->completion_index - row_count / 2, 0, completion_options_size - row_count);

                for (int i = 0; i < row_count; i++) {
                    int l = line_from + i;
                    ERR_CONTINUE(l < 0 || l >= completion_options_size);
                    if (D()->completion_options[l].default_value.get_type() == VariantType::COLOR) {
                        width += icon_area_size.width;
                        break;
                    }
                }

                // Position completion panel
                D()->completion_rect.size.width = width + 2;
                D()->completion_rect.size.height = completion_rows_height;

                if (completion_options_size <= maxlines) {
                    scroll_rectangle_width = 0;
                }

                const Point2 csb_offset = csb->get_offset();
                const int total_height = D()->completion_rect.size.height + csb->get_minimum_size().y;
                const int ajdusted_cursor_y =
                        cursor_pos.y - cursor_insert_offset_y - (get_row_height() - D()->cache.font->get_height()) / 2;

                D()->completion_rect.position.x = cursor_pos.x - completion_base_width - icon_area_width - csb_offset.x;
                if (ajdusted_cursor_y + row_height + total_height > get_size().height &&
                        ajdusted_cursor_y > total_height) {
                    // Completion panel above the cursor line
                    D()->completion_rect.position.y = ajdusted_cursor_y - total_height;
                } else {
                    // Completion panel below the cursor line
                    D()->completion_rect.position.y = ajdusted_cursor_y + row_height;
                    completion_below = true;
                }

                draw_style_box(csb, Rect2(D()->completion_rect.position - csb_offset,
                                            D()->completion_rect.size + csb->get_minimum_size() +
                                                    Size2(scroll_rectangle_width, 0)));

                if (D()->cache.completion_background_color.a > 0.01) {
                    RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                            Rect2(D()->completion_rect.position,
                                    D()->completion_rect.size + Size2(scroll_rectangle_width, 0)),
                            D()->cache.completion_background_color);
                }

                RenderingServer::get_singleton()->canvas_item_add_rect(ci,
                        Rect2(Point2(D()->completion_rect.position.x,
                                      D()->completion_rect.position.y +
                                              (D()->completion_index - line_from) * get_row_height()),
                                Size2(D()->completion_rect.size.width, get_row_height())),
                        D()->cache.completion_selected_color);
                draw_rect_filled(Rect2(D()->completion_rect.position + Vector2(icon_area_size.x + icon_hsep, 0),
                                         Size2(MIN(completion_base_width, D()->completion_rect.size.width -
                                                                                  (icon_area_size.x + icon_hsep)),
                                                 D()->completion_rect.size.height)),
                        D()->cache.completion_existing_color);

                for (int i = 0; i < row_count; i++) {

                    int l = line_from + i;
                    ERR_CONTINUE(l < 0 || l >= completion_options_size);
                    Color text_color = D()->cache.completion_font_color;
                    for (size_t j = 0; j < D()->color_regions.size(); j++) {
                        if (StringUtils::begins_with(StringUtils::from_utf8(D()->completion_options[l].insert_text),
                                    D()->color_regions[j].begin_key)) {
                            text_color = D()->color_regions[j].color;
                        }
                    }
                    int yofs = (get_row_height() - D()->cache.font->get_height()) / 2;
                    Point2 title_pos(D()->completion_rect.position.x, D()->completion_rect.position.y +
                                                                              i * get_row_height() +
                                                                              D()->cache.font->get_ascent() + yofs);

                    // Draw completion icon if it is valid.
                    Ref<Texture> icon = dynamic_ref_cast<Texture>(D()->completion_options[l].icon);
                    Rect2 icon_area(D()->completion_rect.position.x,
                            D()->completion_rect.position.y + i * get_row_height(), icon_area_size.width,
                            icon_area_size.height);
                    if (icon) {
                        const real_t max_scale = 0.7f;
                        const real_t side = max_scale * icon_area.size.width;
                        real_t scale = MIN(side / icon->get_width(), side / icon->get_height());
                        Size2 icon_size = icon->get_size() * scale;
                        draw_texture_rect(
                                icon, Rect2(icon_area.position + (icon_area.size - icon_size) / 2, icon_size));
                    }

                    title_pos.x = icon_area.position.x + icon_area.size.width + icon_hsep;
                    if (D()->completion_options[l].default_value.get_type() == VariantType::COLOR) {
                        draw_rect_filled(Rect2(Point2(D()->completion_rect.position.x +
                                                               D()->completion_rect.size.width - icon_area_size.x,
                                                       icon_area.position.y),
                                                 icon_area_size),
                                (Color)D()->completion_options[l].default_value);
                    }
                    draw_string(D()->cache.font, title_pos, D()->completion_options[l].display, text_color,
                            D()->completion_rect.size.width - (icon_area_size.x + icon_hsep));
                }

                if (scroll_rectangle_width) {
                    // Draw a small scroll rectangle to show a position in the options.
                    float r = (float)maxlines / completion_options_size;
                    float o = (float)line_from / completion_options_size;
                    draw_rect_filled(Rect2(D()->completion_rect.position.x + D()->completion_rect.size.width,
                                             D()->completion_rect.position.y + o * D()->completion_rect.size.y,
                                             scroll_rectangle_width, D()->completion_rect.size.y * r),
                            scrollc);
                }

                D()->completion_line_ofs = line_from;
            }

            // Check to see if the hint should be drawn.
            bool show_hint = false;
            if (is_cursor_visible && !D()->completion_hint.empty()) {
                if (D()->completion_active) {
                    if (completion_below && !callhint_below) {
                        show_hint = true;
                    } else if (!completion_below && callhint_below) {
                        show_hint = true;
                    }
                } else {
                    show_hint = true;
                }
            }

            if (show_hint) {

                Ref<StyleBox> sb = get_theme_stylebox("panel", "TooltipPanel");
                Ref<Font> font = D()->cache.font;
                Color font_color = get_theme_color("font_color", "TooltipLabel");

                int max_w = 0;
                int sc = StringUtils::get_slice_count(D()->completion_hint,'\n');
                int offset = 0;
                int spacing = 0;
                for (int i = 0; i < sc; i++) {

                    StringView l = StringUtils::get_slice(D()->completion_hint,"\n", i);
                    int len = font->get_string_size(l).x;
                    max_w = M_MAX(len, max_w);
                    if (i == 0) {
                        offset = font->get_string_size(StringUtils::substr(l, 0, StringUtils::find(l, c_cursor_marker)))
                                         .x;
                    } else {
                        spacing += D()->cache.line_spacing;
                    }
                }

                Size2 size2 = Size2(max_w, sc * font->get_height() + spacing);
                Size2 minsize = size2 + sb->get_minimum_size();

                if (D()->completion_hint_offset == -0xFFFF) {
                    D()->completion_hint_offset = cursor_pos.x - offset;
                }

                Point2 hint_ofs = Vector2(D()->completion_hint_offset,
                                          cursor_pos.y - cursor_insert_offset_y -
                                                  (get_row_height() - D()->cache.font->get_height()) / 2) +
                                  callhint_offset;

                if (callhint_below) {
                    hint_ofs.y += get_row_height() + sb->get_offset().y;
                } else {
                    hint_ofs.y -= minsize.y + sb->get_offset().y;
                }

                draw_style_box(sb, Rect2(hint_ofs, minsize));

                spacing = 0;
                for (int i = 0; i < sc; i++) {
                    int begin = 0;
                    int end = 0;
                    StringView l = StringUtils::get_slice(D()->completion_hint,"\n", i);
                    //TODO: replace construction of Strings here with 'char' search
                    if (StringUtils::contains(l,c_cursor_marker)) {
                        begin = font->get_string_size(StringUtils::substr(l, 0, StringUtils::find(l, c_cursor_marker)))
                                        .x;
                        end = font->get_string_size(StringUtils::substr(l, 0, StringUtils::rfind(l, c_cursor_marker)))
                                      .x;
                    }

                    char cursor[2] = {c_cursor_marker,0};
                    Point2 round_ofs = hint_ofs + sb->get_offset() +
                                       Vector2(0, font->get_ascent() + font->get_height() * i + spacing);
                    round_ofs = round_ofs.round();
                    draw_string(font, round_ofs, StringUtils::replace(l,cursor,""), font_color);

                    if (end > 0) {
                        Vector2 b = hint_ofs + sb->get_offset() +
                                    Vector2(begin, font->get_height() + font->get_height() * i + spacing - 1);
                        draw_line(b, b + Vector2(end - begin, 0), font_color);
                    }
                    spacing += D()->cache.line_spacing;
                }
            }

            if (has_focus()) {
                OS::get_singleton()->set_ime_active(true);
                OS::get_singleton()->set_ime_position(get_global_position() + cursor_pos + Point2(0, get_row_height()));
            }

        } break;
        case NOTIFICATION_FOCUS_ENTER: {

            if (caret_blink_enabled) {
                caret_blink_timer->start();
            } else {
                draw_caret = true;
            }

            OS::get_singleton()->set_ime_active(true);
            Point2 cursor_pos = Point2(cursor_get_column(), cursor_get_line()) * get_row_height();
            OS::get_singleton()->set_ime_position(get_global_position() + cursor_pos);

        } break;
        case NOTIFICATION_FOCUS_EXIT: {
            if (caret_blink_enabled) {
                caret_blink_timer->stop();
            }

            OS::get_singleton()->set_ime_position(Point2());
            OS::get_singleton()->set_ime_active(false);
            D()->ime_text = "";
            D()->ime_selection = Point2();

            if (D()->deselect_on_focus_loss_enabled && !D()->popup_show) {
                deselect();
            }
            D()->popup_show = false;
        } break;
        case MainLoop::NOTIFICATION_OS_IME_UPDATE: {

            if (has_focus()) {
                D()->ime_text = StringUtils::from_utf8(OS::get_singleton()->get_ime_text());
                D()->ime_selection = OS::get_singleton()->get_ime_selection();
                update();
            }
        } break;
        case Control::NOTIFICATION_DRAG_BEGIN: {
            D()->selection.selecting_mode = PrivateData::Selection::MODE_NONE;
            drag_action = true;
            dragging_minimap = false;
            D()->dragging_selection = false;
            can_drag_minimap = false;
            D()->click_select_held->stop();
        } break;
        case Control::NOTIFICATION_DRAG_END: {
            if (is_drag_successful()) {
                if (D()->selection.drag_attempt) {
                    D()->selection.drag_attempt = false;
                    if (!readonly && !Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
                        D()->_remove_text(D()->selection.from_line, D()->selection.from_column, D()->selection.to_line, D()->selection.to_column);
                        cursor_set_line(D()->selection.from_line, false);
                        cursor_set_column(D()->selection.from_column);
                        D()->selection.active = false;
                        D()->selection.selecting_mode = PrivateData::Selection::MODE_NONE;
                        update();
                    } else if (D()->deselect_on_focus_loss_enabled) {
                        deselect();
    }
}

            } else {
                D()->selection.drag_attempt = false;
            }
            drag_action = false;
            drag_caret_force_displayed = false;
            dragging_minimap = false;
            D()->dragging_selection = false;
            can_drag_minimap = false;
            D()->click_select_held->stop();
        } break;
    }
}

void TextEdit::backspace_at_cursor() {
    if (readonly)
        return;

    if (D()->cursor.column == 0 && D()->cursor.line == 0)
        return;

    int prev_line = D()->cursor.column ? D()->cursor.line : D()->cursor.line - 1;
    int prev_column = D()->cursor.column ? (D()->cursor.column - 1) : (D()->text[ D()->cursor.line - 1].length());

    if (is_line_hidden(D()->cursor.line))
        set_line_as_hidden(prev_line, true);
    if (is_line_set_as_breakpoint(D()->cursor.line)) {
        if (!D()->text.is_breakpoint(prev_line))
            emit_signal("breakpoint_toggled", prev_line);
        set_line_as_breakpoint(prev_line, true);
    }

    if (D()->text.has_info_icon(D()->cursor.line)) {
        set_line_info_icon(prev_line, D()->text.get_info_icon(D()->cursor.line), D()->text.get_info(D()->cursor.line));
    }

    if (auto_brace_completion_enabled && D()->cursor.column > 0 &&
            _is_pair_left_symbol(D()->text[ D()->cursor.line][ D()->cursor.column - 1])) {
        D()->_consume_backspace_for_pair_symbol(prev_line, prev_column);
    } else {
        // Handle space indentation.
        if (D()->cursor.column != 0 && indent_using_spaces) {
            // Check if there are no other chars before cursor, just indentation.
            bool unindent = true;
            int i = 0;
            while (i < D()->cursor.column && i < D()->text[ D()->cursor.line].length()) {
                if (!_is_whitespace(D()->text[ D()->cursor.line][i])) {
                    unindent = false;
                    break;
                }
                i++;
            }

            // Then we can remove all spaces as a single character.
            if (unindent) {
                // We want to remove spaces up to closest indent, or whole indent if cursor is pointing at it.
                int spaces_to_delete = _calculate_spaces_till_next_left_indent(D()->cursor.column);
                prev_column = D()->cursor.column - spaces_to_delete;
                D()->_remove_text(D()->cursor.line, prev_column, D()->cursor.line, D()->cursor.column);
            } else {
                D()->_remove_text(prev_line, prev_column, D()->cursor.line, D()->cursor.column);
            }
        } else {
            D()->_remove_text(prev_line, prev_column, D()->cursor.line, D()->cursor.column);
        }
    }

    cursor_set_line(prev_line, false, true);
    cursor_set_column(prev_column);
}

void TextEdit::indent_right() {

    int start_line;
    int end_line;

    // This value informs us by how much we changed selection position by indenting right.
    // Default is 1 for tab indentation.
    int selection_offset = 1;
    begin_complex_operation();

    if (is_selection_active()) {
        start_line = get_selection_from_line();
        end_line = get_selection_to_line();
    } else {
        start_line = D()->cursor.line;
        end_line = start_line;
    }

    // Ignore if the cursor is not past the first column.
    if (is_selection_active() && get_selection_to_column() == 0) {
        selection_offset = 0;
        end_line--;
    }

    for (int i = start_line; i <= end_line; i++) {
        UIString line_text = StringUtils::from_utf8(get_line(i));
        if (line_text.isEmpty() && is_selection_active()) {
            continue;
        }

        if (indent_using_spaces) {
            // We don't really care where selection is - we just need to know indentation level at the beginning of the
            // line.
            int left = _find_first_non_whitespace_column_of_line(line_text);
            int spaces_to_add = _calculate_spaces_till_next_right_indent(left);
            // Since we will add this much spaces we want move whole selection and cursor by this much.
            selection_offset = spaces_to_add;
            for (int j = 0; j < spaces_to_add; j++)
                line_text = ' ' + line_text;
        } else {
            line_text = '\t' + line_text;
        }
        set_line(i, StringUtils::to_utf8(line_text));
    }

    // Fix selection and cursor being off after shifting selection right.
    if (is_selection_active()) {
        select(D()->selection.from_line, D()->selection.from_column + selection_offset, D()->selection.to_line,
                D()->selection.to_column + selection_offset);
    }
    cursor_set_column(D()->cursor.column + selection_offset, false);
    D()->end_complex_operation();
    update();
}

void TextEdit::indent_left() {

    int start_line;
    int end_line;

    // Moving cursor and selection after unindenting can get tricky because
    // changing content of line can move cursor and selection on it's own (if new line ends before previous position of
    // either), therefore we just remember initial values and at the end of the operation offset them by number of
    // removed characters.
    int removed_characters = 0;
    int initial_selection_end_column = D()->selection.to_column;
    int initial_cursor_column = D()->cursor.column;

    begin_complex_operation();

    if (is_selection_active()) {
        start_line = get_selection_from_line();
        end_line = get_selection_to_line();
    } else {
        start_line = D()->cursor.line;
        end_line = start_line;
    }

    // Ignore if the cursor is not past the first column.
    if (is_selection_active() && get_selection_to_column() == 0) {
        end_line--;
    }
    String first_line_text = get_line(start_line);
    String last_line_text = get_line(end_line);

    for (int i = start_line; i <= end_line; i++) {
        UIString line_text = StringUtils::from_utf8(get_line(i));

        if (StringUtils::begins_with(line_text,"\t")) {
            line_text = StringUtils::substr(line_text,1, line_text.length());
            set_line(i, StringUtils::to_utf8(line_text));
            removed_characters = 1;
        } else if (StringUtils::begins_with(line_text," ")) {
            // When unindenting we aim to remove spaces before line that has selection no matter what is selected,
            // so we start of by finding first non whitespace character of line
            int left = _find_first_non_whitespace_column_of_line(line_text);

            // Here we remove only enough spaces to align text to nearest full multiple of indentation_size.
            // In case where selection begins at the start of indentation_size multiple we remove whole indentation
            // level.
            int spaces_to_remove = _calculate_spaces_till_next_left_indent(left);

            line_text = StringUtils::substr(line_text,spaces_to_remove, line_text.length());
            set_line(i, StringUtils::to_utf8(line_text));
            removed_characters = spaces_to_remove;
        }
    }

    if (is_selection_active()) {
        // Fix selection being off by one on the first line.
        if (first_line_text != get_line(start_line)) {
            select(D()->selection.from_line, D()->selection.from_column - removed_characters, D()->selection.to_line,
                    initial_selection_end_column);
    }
        // Fix selection being off by one on the last line.
        if (last_line_text != get_line(end_line)) {
            select(D()->selection.from_line, D()->selection.from_column, D()->selection.to_line,
                    initial_selection_end_column - removed_characters);
        }
    }
    cursor_set_column(initial_cursor_column - removed_characters, false);
    D()->end_complex_operation();
    update();
}

int TextEdit::_calculate_spaces_till_next_left_indent(int column) {
    int spaces_till_indent = column % indent_size;
    if (spaces_till_indent == 0)
        spaces_till_indent = indent_size;
    return spaces_till_indent;
}

int TextEdit::_calculate_spaces_till_next_right_indent(int column) {
    return indent_size - column % indent_size;
}

void TextEdit::_get_mouse_pos(const Point2i &p_mouse, int &r_row, int &r_col) const {

    float rows = p_mouse.y;
    rows -= D()->cache.style_normal->get_margin(Margin::Top);
    rows /= get_row_height();
    rows += get_v_scroll_offset();
    int first_vis_line = get_first_visible_line();
    int row = first_vis_line + Math::floor(rows);
    int wrap_index = 0;

    if (is_wrap_enabled() || is_hiding_enabled()) {

        int f_ofs = num_lines_from_rows(first_vis_line, D()->cursor.wrap_ofs, rows + (1 * SGN(rows)), wrap_index) - 1;
        if (rows < 0)
            row = first_vis_line - f_ofs;
        else
            row = first_vis_line + f_ofs;
    }

    if (row < 0)
        row = 0; // TODO.

    int col = 0;

    if (row >= D()->text.size()) {

        row = D()->text.size() - 1;
        col = D()->text[row].size();
    } else {

        int colx = p_mouse.x - (D()->cache.style_normal->get_margin(Margin::Left) + D()->cache.line_number_w +
                                       D()->cache.breakpoint_gutter_width + D()->cache.fold_gutter_width +
                                       D()->cache.info_gutter_width);
        colx += D()->cursor.x_ofs;
        col = get_char_pos_for_line(colx, row, wrap_index);
        if (is_wrap_enabled() && wrap_index < get_line_wrap_count(row)) {
            // Move back one if we are at the end of the row.
            Vector<UIString> rows2 = get_wrap_rows_text(row);
            int row_end_col = 0;
            for (int i = 0; i < wrap_index + 1; i++) {
                row_end_col += rows2[i].length();
            }
            if (col >= row_end_col)
                col -= 1;
        }
    }

    r_row = row;
    r_col = col;
}

Vector2i TextEdit::_get_cursor_pixel_pos() {
    adjust_viewport_to_cursor();
    int row = (D()->cursor.line - get_first_visible_line() - D()->cursor.wrap_ofs);
    // Correct for hidden and wrapped lines
    for (int i = get_first_visible_line(); i < D()->cursor.line; i++) {
        if (is_line_hidden(i)) {
            row -= 1;
            continue;
        }
        row += get_line_wrap_count(i);
    }
    // Row might be wrapped. Adjust row and r_column
    Vector<UIString> rows2 = get_wrap_rows_text(D()->cursor.line);
    while (rows2.size() > 1) {
        if (D()->cursor.column >= rows2[0].length()) {
            D()->cursor.column -= rows2[0].length();
            rows2.pop_front();
            row++;
        } else {
            break;
        }
    }

    // Calculate final pixel position
    int y = (row - get_v_scroll_offset() + 1 /*Bottom of line*/) * get_row_height();
    int x = D()->cache.style_normal->get_margin(Margin::Left) + D()->cache.line_number_w +
            D()->cache.breakpoint_gutter_width + D()->cache.fold_gutter_width + D()->cache.info_gutter_width -
            D()->cursor.x_ofs;
    int ix = 0;
    while (ix < rows2[0].size() && ix < D()->cursor.column) {
        if (D()->cache.font != nullptr) {
            x += D()->cache.font->get_char_size(rows2[0][ix]).width;
        }
        ix++;
    }
    x += get_indent_level(D()->cursor.line) * D()->cache.font->get_char_size(' ').width;

    return Vector2i(x, y);
}

void TextEdit::_get_minimap_mouse_row(const Point2i &p_mouse, int &r_row) const {

    float rows = p_mouse.y;
    rows -= D()->cache.style_normal->get_margin(Margin::Top);
    rows /= (minimap_char_size.y + minimap_line_spacing);
    rows += get_v_scroll_offset();

    // calculate visible lines
    int minimap_visible_lines = _get_minimap_visible_rows();
    int visible_rows = get_visible_rows() + 1;
    int first_visible_line = get_first_visible_line() - 1;
    int draw_amount = visible_rows + (smooth_scroll_enabled ? 1 : 0);
    draw_amount += get_line_wrap_count(first_visible_line + 1);
    int minimap_line_height = (minimap_char_size.y + minimap_line_spacing);

    // calculate viewport size and y offset
    int viewport_height = (draw_amount - 1) * minimap_line_height;
    int control_height = _get_control_height() - viewport_height;
    int viewport_offset_y = round(get_scroll_pos_for_line(first_visible_line) * control_height) /
                            ((v_scroll->get_max() <= minimap_visible_lines) ? (minimap_visible_lines - draw_amount) :
                                                                              (v_scroll->get_max() - draw_amount));

    // calculate the first line.
    int num_lines_before = round((viewport_offset_y) / minimap_line_height);
    int wi;
    int minimap_line = (v_scroll->get_max() <= minimap_visible_lines) ? -1 : first_visible_line;
    if (first_visible_line > 0 && minimap_line >= 0) {
        minimap_line -= num_lines_from_rows(first_visible_line, 0, -num_lines_before, wi);
        minimap_line -= (minimap_line > 0 && smooth_scroll_enabled ? 1 : 0);
    } else {
        minimap_line = 0;
    }

    int row = minimap_line + Math::floor(rows);
    int wrap_index = 0;

    if (is_wrap_enabled() || is_hiding_enabled()) {

        int f_ofs = num_lines_from_rows(minimap_line, D()->cursor.wrap_ofs, rows + (1 * SGN(rows)), wrap_index) - 1;
        if (rows < 0) {
            row = minimap_line - f_ofs;
        } else {
            row = minimap_line + f_ofs;
        }
    }

    if (row < 0) {
        row = 0;
    }

    if (row >= D()->text.size()) {
        row = D()->text.size() - 1;
    }

    r_row = row;
}

void TextEdit::_gui_input(const Ref<InputEvent> &p_gui_input) {

    using Selection = PrivateData::Selection;
    double prev_v_scroll = v_scroll->get_value();
    double prev_h_scroll = h_scroll->get_value();

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_gui_input);

    if (mb) {
        if (D()->completion_active && D()->completion_rect.has_point(mb->get_position())) {

            if (!mb->is_pressed())
                return;

            if (mb->get_button_index() == BUTTON_WHEEL_UP) {
                if (D()->completion_index > 0) {
                    D()->completion_index--;
                    D()->completion_current = D()->completion_options[D()->completion_index];
                    update();
                }
            }
            if (mb->get_button_index() == BUTTON_WHEEL_DOWN) {

                if (D()->completion_index < D()->completion_options.size() - 1) {
                    D()->completion_index++;
                    D()->completion_current = D()->completion_options[D()->completion_index];
                    update();
                }
            }

            if (mb->get_button_index() == BUTTON_LEFT) {

                D()->completion_index =
                        CLAMP<int>(D()->completion_line_ofs +
                                           (mb->get_position().y - D()->completion_rect.position.y) / get_row_height(),
                                0, D()->completion_options.size() - 1);

                D()->completion_current = D()->completion_options[D()->completion_index];
                update();
                if (mb->is_doubleclick())
                    _confirm_completion();
            }
            return;
        } else {
            D()->_cancel_completion();
            D()->_cancel_code_hint();
        }

        if (mb->is_pressed()) {

            if (mb->get_button_index() == BUTTON_WHEEL_UP && !mb->get_command()) {
                if (mb->get_shift()) {
                    h_scroll->set_value(h_scroll->get_value() - (100 * mb->get_factor()));
                } else if (mb->get_alt()) {
                    // Scroll 5 times as fast as normal (like in Visual Studio Code).
                    _scroll_up(15 * mb->get_factor());
                } else if (v_scroll->is_visible()) {
                    // Scroll 3 lines.
                    _scroll_up(3 * mb->get_factor());
                }
            }
            if (mb->get_button_index() == BUTTON_WHEEL_DOWN && !mb->get_command()) {
                if (mb->get_shift()) {
                    h_scroll->set_value(h_scroll->get_value() + (100 * mb->get_factor()));
                } else if (mb->get_alt()) {
                    // Scroll 5 times as fast as normal (like in Visual Studio Code).
                    _scroll_down(15 * mb->get_factor());
                } else {
                    // Scroll 3 lines.
                    _scroll_down(3 * mb->get_factor());
                }
            }
            if (mb->get_button_index() == BUTTON_WHEEL_LEFT) {
                h_scroll->set_value(h_scroll->get_value() - (100 * mb->get_factor()));
            }
            if (mb->get_button_index() == BUTTON_WHEEL_RIGHT) {
                h_scroll->set_value(h_scroll->get_value() + (100 * mb->get_factor()));
            }
            if (mb->get_button_index() == BUTTON_LEFT) {

                _reset_caret_blink_timer();

                int row, col;
                _get_mouse_pos(Point2i(mb->get_position().x, mb->get_position().y), row, col);

                // Toggle breakpoint on gutter click.
                if (draw_breakpoint_gutter) {
                    int gutter = D()->cache.style_normal->get_margin(Margin::Left);
                    if (mb->get_position().x > gutter - 6 &&
                            mb->get_position().x <= gutter + D()->cache.breakpoint_gutter_width - 3) {
                        set_line_as_breakpoint(row, !is_line_set_as_breakpoint(row));
                        emit_signal("breakpoint_toggled", row);
                        return;
                    }
                }

                // Emit info clicked.
                if (draw_info_gutter && D()->text.has_info_icon(row)) {
                    int left_margin = D()->cache.style_normal->get_margin(Margin::Left);
                    int gutter_left = left_margin + D()->cache.breakpoint_gutter_width;
                    if (mb->get_position().x > gutter_left - 6 &&
                            mb->get_position().x <= gutter_left + D()->cache.info_gutter_width - 3) {
                        emit_signal("info_clicked", row, D()->text.get_info(row));
                        return;
                    }
                }

                // Toggle fold on gutter click if can.
                if (draw_fold_gutter) {

                    int left_margin = D()->cache.style_normal->get_margin(Margin::Left);
                    int gutter_left = left_margin + D()->cache.breakpoint_gutter_width + D()->cache.line_number_w +
                                      D()->cache.info_gutter_width;
                    if (mb->get_position().x > gutter_left - 6 &&
                            mb->get_position().x <= gutter_left + D()->cache.fold_gutter_width - 3) {
                        if (is_folded(row)) {
                            unfold_line(row);
                        } else if (can_fold(row)) {
                            fold_line(row);
                        }
                        return;
                    }
                }

                // Unfold on folded icon click.
                if (is_folded(row)) {
                    int line_width = D()->text.get_line_width(row);
                    line_width += D()->cache.style_normal->get_margin(Margin::Left) + D()->cache.line_number_w +
                                  D()->cache.breakpoint_gutter_width + D()->cache.info_gutter_width +
                                  D()->cache.fold_gutter_width - D()->cursor.x_ofs;
                    if (mb->get_position().x > line_width - 3 &&
                            mb->get_position().x <= line_width + D()->cache.folded_eol_icon->get_width() + 3) {
                        unfold_line(row);
                        return;
                    }
                }

                // minimap
                if (draw_minimap) {
                    _update_minimap_click();
                    if (dragging_minimap) {
                        return;
                    }
                }

                int prev_col = D()->cursor.column;
                int prev_line = D()->cursor.line;

                cursor_set_line(row, false, false);
                cursor_set_column(col);
                D()->selection.drag_attempt = false;

                if (mb->get_shift() && (D()->cursor.column != prev_col || D()->cursor.line != prev_line)) {

                    if (!D()->selection.active) {
                        D()->selection.active = true;
                        D()->selection.selecting_mode = PrivateData::Selection::MODE_POINTER;
                        D()->selection.from_column = prev_col;
                        D()->selection.from_line = prev_line;
                        D()->selection.to_column = D()->cursor.column;
                        D()->selection.to_line = D()->cursor.line;

                        if (D()->selection.from_line > D()->selection.to_line ||
                                (D()->selection.from_line == D()->selection.to_line &&
                                        D()->selection.from_column > D()->selection.to_column)) {
                            SWAP(D()->selection.from_column, D()->selection.to_column);
                            SWAP(D()->selection.from_line, D()->selection.to_line);
                            D()->selection.shiftclick_left = false;
                        } else {
                            D()->selection.shiftclick_left = true;
                        }
                        D()->selection.selecting_line = prev_line;
                        D()->selection.selecting_column = prev_col;
                        update();
                    } else {

                        if (D()->cursor.line < D()->selection.selecting_line ||
                                (D()->cursor.line == D()->selection.selecting_line &&
                                        D()->cursor.column < D()->selection.selecting_column)) {

                            if (D()->selection.shiftclick_left) {
                                SWAP(D()->selection.from_column, D()->selection.to_column);
                                SWAP(D()->selection.from_line, D()->selection.to_line);
                                D()->selection.shiftclick_left = !D()->selection.shiftclick_left;
                            }
                            D()->selection.from_column = D()->cursor.column;
                            D()->selection.from_line = D()->cursor.line;

                        } else if (D()->cursor.line > D()->selection.selecting_line ||
                                   (D()->cursor.line == D()->selection.selecting_line &&
                                           D()->cursor.column > D()->selection.selecting_column)) {

                            if (!D()->selection.shiftclick_left) {
                                SWAP(D()->selection.from_column, D()->selection.to_column);
                                SWAP(D()->selection.from_line, D()->selection.to_line);
                                D()->selection.shiftclick_left = !D()->selection.shiftclick_left;
                            }
                            D()->selection.to_column = D()->cursor.column;
                            D()->selection.to_line = D()->cursor.line;

                        } else {
                            D()->selection.active = false;
                        }

                        update();
                    }

                } else if (is_mouse_over_selection()) {
                    D()->selection.selecting_mode = PrivateData::Selection::MODE_NONE;
                    D()->selection.drag_attempt = true;
                } else {

                    D()->selection.active = false;
                    D()->selection.selecting_mode = PrivateData::Selection::MODE_POINTER;
                    D()->selection.selecting_line = row;
                    D()->selection.selecting_column = col;
                }
                const int triple_click_timeout = 600;
                const int triple_click_tolerance = 5;

                if (!mb->is_doubleclick() &&
                        (OS::get_singleton()->get_ticks_msec() - last_dblclk) < triple_click_timeout &&
                        mb->get_position().distance_to(last_dblclk_pos) < triple_click_tolerance) {

                    // Triple-click select line.
                    D()->selection.selecting_mode = PrivateData::Selection::MODE_LINE;
                    D()->_update_selection_mode_line();
                    last_dblclk = 0;
                } else if (mb->is_doubleclick() && D()->text[D()->cursor.line].length()) {

                    // Double-click select word.
                    D()->selection.selecting_mode = PrivateData::Selection::MODE_WORD;
                    D()->_update_selection_mode_word();
                    last_dblclk = OS::get_singleton()->get_ticks_msec();
                    last_dblclk_pos = mb->get_position();
                }

                update();
            }
            if (is_middle_mouse_paste_enabled() && mb->get_button_index() == BUTTON_MIDDLE && !readonly && OS::get_singleton()->has_feature("primary_clipboard")) {
                String paste_buffer = OS::get_singleton()->get_clipboard_primary();

                int row, col;
                _get_mouse_pos(Point2i(mb->get_position().x, mb->get_position().y), row, col);
                begin_complex_operation();

                deselect();
                cursor_set_line(row, true, false);
                cursor_set_column(col);
                if (!paste_buffer.empty()) {
                    D()->_insert_text_at_cursor(StringUtils::from_utf8(paste_buffer));
                }
                end_complex_operation();

                grab_focus();
                update();
            }
            if (mb->get_button_index() == BUTTON_RIGHT && context_menu_enabled) {

                _reset_caret_blink_timer();

                int row, col;
                _get_mouse_pos(Point2i(mb->get_position().x, mb->get_position().y), row, col);

                if (is_right_click_moving_caret()) {
                    if (is_selection_active()) {

                        int from_line = get_selection_from_line();
                        int to_line = get_selection_to_line();
                        int from_column = get_selection_from_column();
                        int to_column = get_selection_to_column();

                        if (row < from_line || row > to_line || (row == from_line && col < from_column) ||
                                (row == to_line && col > to_column)) {
                            // Right click is outside the selected text.
                            deselect();
                        }
                    }
                    if (!is_selection_active()) {
                        cursor_set_line(row, true, false);
                        cursor_set_column(col);
                    }
                }

                popup_show = true;
                if (!readonly) {
                    menu->set_item_disabled(menu->get_item_index(MENU_UNDO), !has_undo());
                    menu->set_item_disabled(menu->get_item_index(MENU_REDO), !has_redo());
                }
                menu->set_position(get_global_transform().xform(get_local_mouse_position()));
                menu->set_size(Vector2(1, 1));
                menu->set_scale(get_global_transform().get_scale());
                menu->popup();
            }
        } else {

            if (mb->get_button_index() == BUTTON_LEFT) {
                if (D()->selection.drag_attempt && D()->selection.selecting_mode == PrivateData::Selection::MODE_NONE && is_mouse_over_selection()) {
                    D()->selection.active = false;
                }
                if (mb->get_command() && !D()->highlighted_word.isEmpty()) {
                    int row, col;
                    _get_mouse_pos(Point2i(mb->get_position().x, mb->get_position().y), row, col);

                    emit_signal("symbol_lookup", StringUtils::to_utf8(D()->highlighted_word), row, col);
                    return;
                }
                dragging_minimap = false;
                D()->dragging_selection = false;
                can_drag_minimap = false;
                D()->click_select_held->stop();
                if (!drag_action) {
                     D()->selection.drag_attempt = false;
                }
                if (OS::get_singleton()->has_feature("primary_clipboard")) {
                    OS::get_singleton()->set_clipboard_primary(get_selection_text());
                }
            }

            // Notify to show soft keyboard.
            notification(NOTIFICATION_FOCUS_ENTER);
        }
    }

    const Ref<InputEventPanGesture> pan_gesture = dynamic_ref_cast<InputEventPanGesture>(p_gui_input);
    if (pan_gesture) {

        const real_t delta = pan_gesture->get_delta().y;
        if (delta < 0) {
            _scroll_up(-delta);
        } else {
            _scroll_down(delta);
        }
        h_scroll->set_value(h_scroll->get_value() + pan_gesture->get_delta().x * 100);
        if (v_scroll->get_value() != prev_v_scroll || h_scroll->get_value() != prev_h_scroll)
            accept_event(); // Accept event if scroll changed.

        return;
    }

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_gui_input);

    if (mm) {

        if (select_identifiers_enabled) {
            if (!dragging_minimap && !D()->dragging_selection && mm->get_command() && mm->get_button_mask() == 0) {

                UIString new_word(StringUtils::from_utf8(get_word_at_pos(mm->get_position())));
                if (new_word != D()->highlighted_word) {
                    D()->highlighted_word = new_word;
                    update();
                }
            } else {
                if (!D()->highlighted_word.isEmpty()) {
                    D()->highlighted_word = UIString();
                    update();
                }
            }
        }

        if (draw_minimap && !D()->dragging_selection) {
            _update_minimap_hover();
        }

        if (mm->get_button_mask() & BUTTON_MASK_LEFT &&
                get_viewport()->gui_get_drag_data() == Variant()) { // Ignore if dragging.
            _reset_caret_blink_timer();

            if (draw_minimap && !D()->dragging_selection) {
                _update_minimap_drag();
            }

            if (!dragging_minimap) {
            switch (D()->selection.selecting_mode) {
                case PrivateData::Selection::MODE_POINTER: {
                    D()->_update_selection_mode_pointer();
                } break;
                case PrivateData::Selection::MODE_WORD: {
                    D()->_update_selection_mode_word();
                } break;
                case PrivateData::Selection::MODE_LINE: {
                    D()->_update_selection_mode_line();
                } break;
                default: {
                    break;
                }
            }
        }
        }
        if (drag_action && can_drop_data(mm->get_position(), get_viewport()->gui_get_drag_data())) {
            drag_caret_force_displayed = true;
            Point2 mp = get_local_mouse_position();
            int row, col;
            _get_mouse_pos(Point2i(mp.x, mp.y), row, col);
            cursor_set_line(row, true);
            cursor_set_column(col);
            if (row <= get_first_visible_line()) {
                _scroll_lines_up();
            } else if (row >= get_last_full_visible_line()) {
                _scroll_lines_down();
            }
            D()->dragging_selection = true;
            update();
    }
    }

    if (v_scroll->get_value() != prev_v_scroll || h_scroll->get_value() != prev_h_scroll)
        accept_event(); // Accept event if scroll changed.

    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_gui_input);

    if (k) {

        k = dynamic_ref_cast<InputEventKey>(k->duplicate()); // It will be modified later on.

#ifdef OSX_ENABLED
        if (k->get_scancode() == KEY_META) {
#else
        if (k->get_keycode() == KEY_CONTROL) {

#endif
            if (select_identifiers_enabled) {

                if (k->is_pressed() && !dragging_minimap && !D()->dragging_selection) {

                    D()->highlighted_word = StringUtils::from_utf8(get_word_at_pos(get_local_mouse_position()));
                    update();

                } else {
                    D()->highlighted_word.clear();
                    update();
                }
            }
        }

        if (!k->is_pressed())
            return;

        if (D()->completion_active) {
            if (readonly)
                return;

            bool valid = true;
            if (k->get_command() || k->get_metakey())
                valid = false;

            if (valid) {

                if (!k->get_alt()) {
                    if (k->get_keycode() == KEY_UP) {
                        D()->completion_key_up();

                        update();
                        accept_event();
                        return;
                    }

                    if (k->get_keycode() == KEY_DOWN) {
                        D()->completion_key_down();

                        update();
                        accept_event();
                        return;
                    }

                    if (k->get_keycode() == KEY_PAGEUP) {
                        D()->completion_key_page_up();
                        update();
                        accept_event();
                        return;
                    }

                    if (k->get_keycode() == KEY_PAGEDOWN) {
                        D()->completion_key_page_down();

                        update();
                        accept_event();
                        return;
                    }

                    if (k->get_keycode() == KEY_HOME && D()->completion_key_home()) {

                        update();
                        accept_event();
                        return;
                    }

                    if (k->get_keycode() == KEY_END && D()->completion_key_end()) {

                        update();
                        accept_event();
                        return;
                    }

                    if (k->get_keycode() == KEY_KP_ENTER || k->get_keycode() == KEY_ENTER ||
                            k->get_keycode() == KEY_TAB) {

                        _confirm_completion();
                        accept_event();
                        return;
                    }

                    if (k->get_keycode() == KEY_BACKSPACE) {

                        _reset_caret_blink_timer();

                        backspace_at_cursor();
                        _update_completion_candidates();
                        accept_event();
                        return;
                    }

                    if (k->get_keycode() == KEY_SHIFT) {
                        accept_event();
                        return;
                    }
                }

                if (k->get_unicode() > 32) {

                    _reset_caret_blink_timer();

                    const CharType chr = (CharType)k->get_unicode();
                    if (auto_brace_completion_enabled && _is_pair_symbol(chr)) {
                        D()->_consume_pair_symbol(chr);
                    } else {

                        // Remove the old character if in insert mode.
                        if (insert_mode) {
                            begin_complex_operation();

                            // Make sure we don't try and remove empty space.
                            if (D()->cursor.column < get_line(D()->cursor.line).length()) {
                                D()->_remove_text(
                                        D()->cursor.line, D()->cursor.column, D()->cursor.line, D()->cursor.column + 1);
                            }
                        }

                        D()->_insert_text_at_cursor(UIString(chr));

                        if (insert_mode) {
                            D()->end_complex_operation();
                        }
                    }
                    _update_completion_candidates();
                    accept_event();

                    return;
                }
            }

            D()->_cancel_completion();
        }

        /* TEST CONTROL FIRST! */

        // Some remaps for duplicate functions.
        if (k->get_command() && !k->get_shift() && !k->get_alt() && !k->get_metakey() &&
                k->get_keycode() == KEY_INSERT) {

            k->set_keycode(KEY_C);
        }
        if (!k->get_command() && k->get_shift() && !k->get_alt() && !k->get_metakey() &&
                k->get_keycode() == KEY_INSERT) {

            k->set_keycode(KEY_V);
            k->set_command(true);
            k->set_shift(false);
        }
#ifdef APPLE_STYLE_KEYS
        if (k->get_control() && !k->get_shift() && !k->get_alt() && !k->get_command()) {
            uint32_t remap_key = KEY_UNKNOWN;
            switch (k->get_scancode()) {
                case KEY_F: {
                    remap_key = KEY_RIGHT;
                } break;
                case KEY_B: {
                    remap_key = KEY_LEFT;
                } break;
                case KEY_P: {
                    remap_key = KEY_UP;
                } break;
                case KEY_N: {
                    remap_key = KEY_DOWN;
                } break;
                case KEY_D: {
                    remap_key = KEY_DELETE;
                } break;
                case KEY_H: {
                    remap_key = KEY_BACKSPACE;
                } break;
            }

            if (remap_key != KEY_UNKNOWN) {
                k->set_keycode(remap_key);
                k->set_control(false);
            }
        }
#endif

        _reset_caret_blink_timer();

        // Save here for insert mode as well as arrow navigation, just in case it is cleared in the following section.
        bool had_selection = D()->selection.active;

        // Stuff to do when selection is active.
        if (!readonly && D()->selection.active) {

            bool clear = false;
            bool unselect = false;
            bool dobreak = false;

            switch (k->get_keycode()) {

                case KEY_TAB: {
                    if (k->get_shift()) {
                        indent_left();
                    } else {
                        indent_right();
                    }
                    dobreak = true;
                    accept_event();
                } break;
                case KEY_X:
                case KEY_C:
                    // Special keys often used with control, wait.
                    clear = (!k->get_command() || k->get_shift() || k->get_alt());
                    break;
                case KEY_DELETE:
                    if (!k->get_shift()) {
                        accept_event();
                        clear = true;
                        dobreak = true;
                    } else if (k->get_command() || k->get_alt()) {
                        dobreak = true;
                    }
                    break;
                case KEY_BACKSPACE:
                    accept_event();
                    clear = true;
                    dobreak = true;
                    break;
                case KEY_LEFT:
                case KEY_RIGHT:
                case KEY_UP:
                case KEY_DOWN:
                case KEY_PAGEUP:
                case KEY_PAGEDOWN:
                case KEY_HOME:
                case KEY_END:
                    // Ignore arrows if any modifiers are held (shift = selecting, others may be used for editor
                    // hotkeys).
                    if (k->get_command() || k->get_shift() || k->get_alt())
                        break;
                    unselect = true;
                    break;

                default:
                    if (k->get_unicode() >= 32 && !k->get_command() && !k->get_alt() && !k->get_metakey())
                        clear = true;
                    if (auto_brace_completion_enabled && _is_pair_left_symbol(k->get_unicode()))
                        clear = false;
            }

            if (unselect) {
                D()->selection.active = false;
                D()->selection.selecting_mode = Selection::MODE_NONE;
                update();
            }
            if (clear) {

                if (!dobreak) {
                    begin_complex_operation();
                }
                D()->selection.active = false;
                update();
                D()->_remove_text(D()->selection.from_line, D()->selection.from_column, D()->selection.to_line,
                        D()->selection.to_column);
                cursor_set_line(D()->selection.from_line, false, false);
                cursor_set_column(D()->selection.from_column);
                update();
            }
            if (dobreak)
                return;
        }

        D()->selection.selecting_text = false;

        bool scancode_handled = true;

        // Special scancode test.

        switch (k->get_keycode()) {

            case KEY_KP_ENTER:
            case KEY_ENTER: {

                if (readonly)
                    break;

                UIString ins("\n");

                // Keep indentation.
                int space_count = 0;
                for (int i = 0; i < D()->cursor.column; i++) {
                    if (D()->text[ D()->cursor.line][i] == '\t') {
                        if (indent_using_spaces) {
                            ins += D()->space_indent;
                        } else {
                            ins += UIString("\t");
                        }
                        space_count = 0;
                    } else if (D()->text[ D()->cursor.line][i] == ' ') {
                        space_count++;

                        if (space_count == indent_size) {
                            if (indent_using_spaces) {
                                ins += D()->space_indent;
                            } else {
                                ins += '\t';
                            }
                            space_count = 0;
                        }
                    } else {
                        break;
                    }
                }

                if (is_folded(D()->cursor.line))
                    unfold_line(D()->cursor.line);

                bool brace_indent = false;

                // No need to indent if we are going upwards.
                if (auto_indent && !(k->get_command() && k->get_shift())) {
                    // Indent once again if previous line will end with ':','{','[','(' and the line is not a comment
                    // (i.e. colon/brace precedes current cursor position).
                    if (D()->cursor.column > 0) {
                        const Map<int, TextColorRegionInfo> &cri_map =
                                D()->text.get_color_region_info(D()->cursor.line);
                        bool indent_char_found = false;
                        bool should_indent = false;
                        CharType indent_char = ':';
                        const auto &line_ref(D()->text[ D()->cursor.line]);
                        CharType c = D()->cursor.column<line_ref.size() ? line_ref[ D()->cursor.column] : CharType(0);

                        for (int i = 0; i < D()->cursor.column; i++) {
                            c = D()->text[ D()->cursor.line][i];
                            switch (c.toLatin1()) {
                                case ':':
                                case '{':
                                case '[':
                                case '(':
                                    indent_char_found = true;
                                    should_indent = true;
                                    indent_char = c;
                                    continue;
                            }

                            if (indent_char_found && cri_map.contains(i) &&
                                    (D()->color_regions[cri_map.at(i).region].begin_key == "#" ||
                                            D()->color_regions[cri_map.at(i).region].begin_key == "//")) {

                                should_indent = true;
                                break;
                            } else if (indent_char_found && !_is_whitespace(c)) {
                                should_indent = false;
                                indent_char_found = false;
                            }
                        }

                        if (!is_line_comment(D()->cursor.line) && should_indent) {
                            if (indent_using_spaces) {
                                ins += D()->space_indent;
                            } else {
                                ins += "\t";
                            }

                            // No need to move the brace below if we are not taking the text with us.
                            CharType closing_char = _get_right_pair_symbol(indent_char);
                            if ((closing_char != nullptr) &&
                                    (closing_char == D()->text[D()->cursor.line][D()->cursor.column]) &&
                                    !k->get_command()) {
                                brace_indent = true;
                                ins += "\n" + ins.mid(1, ins.length() - 2);
                            }
                        }
                    }
                }
                begin_complex_operation();
                bool first_line = false;
                if (k->get_command()) {
                    if (k->get_shift()) {
                        if (D()->cursor.line > 0) {
                            cursor_set_line(D()->cursor.line - 1);
                            cursor_set_column(D()->text[D()->cursor.line].length());
                        } else {
                            cursor_set_column(0);
                            first_line = true;
                        }
                    } else {
                        cursor_set_column(D()->text[D()->cursor.line].length());
                    }
                }

                insert_text_at_cursor_ui(ins);

                if (first_line) {
                    cursor_set_line(0);
                } else if (brace_indent) {
                    cursor_set_line(D()->cursor.line - 1);
                    cursor_set_column(D()->text[D()->cursor.line].length());
                }
                D()->end_complex_operation();
            } break;
            case KEY_ESCAPE: {
                if (!D()->completion_hint.empty()) {
                    D()->completion_hint = "";
                    update();
                } else {
                    scancode_handled = false;
                }
            } break;
            case KEY_TAB: {
                if (k->get_command())
                    break; // Avoid tab when command.

                if (readonly)
                    break;

                if (is_selection_active()) {
                    if (k->get_shift()) {
                        indent_left();
                    } else {
                        indent_right();
                    }
                } else {
                    if (k->get_shift()) {

                        // Simple unindent.
                        int cc = D()->cursor.column;
                        const UIString &line = D()->text[D()->cursor.line];

                        int left = _find_first_non_whitespace_column_of_line(line);
                        cc = MIN(cc, left);

                        while (cc < indent_size && cc < left && line[cc] == ' ')
                            cc++;

                        if (cc > 0 && cc <= D()->text[D()->cursor.line].length()) {
                            if (D()->text[D()->cursor.line][cc - 1] == '\t') {
                                // Tabs unindentation.
                                D()->_remove_text(D()->cursor.line, cc - 1, D()->cursor.line, cc);
                                if (D()->cursor.column >= left)
                                    cursor_set_column(M_MAX(0, D()->cursor.column - 1));
                                update();
                            } else {
                                // Spaces unindentation.
                                int spaces_to_remove = _calculate_spaces_till_next_left_indent(cc);
                                if (spaces_to_remove > 0) {
                                    D()->_remove_text(D()->cursor.line, cc - spaces_to_remove, D()->cursor.line, cc);
                                    if (D()->cursor.column > left - spaces_to_remove) // Inside text?
                                        cursor_set_column(M_MAX(0, D()->cursor.column - spaces_to_remove));
                                    update();
                                }
                            }
                        } else if (cc == 0 && line.length() > 0 && line[0] == '\t') {
                            D()->_remove_text(D()->cursor.line, 0, D()->cursor.line, 1);
                            update();
                        }
                    } else {
                        // Simple indent.
                        if (indent_using_spaces) {
                            // Insert only as much spaces as needed till next indentation level.
                            int spaces_to_add = _calculate_spaces_till_next_right_indent(D()->cursor.column);
                            UIString indent_to_insert = UIString();
                            for (int i = 0; i < spaces_to_add; i++)
                                indent_to_insert = ' ' + indent_to_insert;
                            D()->_insert_text_at_cursor(indent_to_insert);
                        } else {
                            D()->_insert_text_at_cursor(UIString("\t"));
                        }
                    }
                }

            } break;
            case KEY_BACKSPACE: {
                if (readonly)
                    break;

#ifdef APPLE_STYLE_KEYS
                if (k->get_alt() && D()->cursor.column > 1) {
#else
                if (k->get_alt()) {
                    scancode_handled = false;
                    break;
                } else if (k->get_command() && D()->cursor.column > 1) {
#endif
                    int line = D()->cursor.line;
                    int column = D()->cursor.column;

                    // Check if we are removing a single whitespace, if so remove it and the next char type,
                    // else we just remove the whitespace.
                    bool only_whitespace = false;
                    if (_is_whitespace(D()->text[line][column - 1]) && _is_whitespace(D()->text[line][column - 2])) {
                        only_whitespace = true;
                    } else if (_is_whitespace(D()->text[line][column - 1])) {
                        // Remove the single whitespace.
                        column--;
                    }

                    // Check if its a text char.
                    bool only_char = (_te_is_text_char(D()->text[line][column - 1]) && !only_whitespace);

                    // If its not whitespace or char then symbol.
                    bool only_symbols = !(only_whitespace || only_char);

                    while (column > 0) {
                        bool is_whitespace = _is_whitespace(D()->text[line][column - 1]);
                        bool is_text_char = _te_is_text_char(D()->text[line][column - 1]);

                        if (only_whitespace && !is_whitespace) {
                            break;
                        } else if (only_char && !is_text_char) {
                            break;
                        } else if (only_symbols && (is_whitespace || is_text_char)) {
                            break;
                        }
                        column--;
                    }

                    D()->_remove_text(line, column, D()->cursor.line, D()->cursor.column);

                    cursor_set_line(line);
                    cursor_set_column(column);

#ifdef APPLE_STYLE_KEYS
                } else if (k->get_command()) {
                    int cursor_current_column = D()->cursor.column;
                    D()->cursor.column = 0;
                    D()->_remove_text(D()->cursor.line, 0, D()->cursor.line, cursor_current_column);
#endif
                } else {
                    if (D()->cursor.line > 0 && is_line_hidden(D()->cursor.line - 1))
                        unfold_line(D()->cursor.line - 1);
                    backspace_at_cursor();
                }

            } break;
            case KEY_KP_4: {
                if (k->get_unicode() != 0) {
                    scancode_handled = false;
                    break;
                }
                [[fallthrough]];
            }
            case KEY_LEFT: {

                if (k->get_shift()) {
                    _pre_shift_selection();
                } else if (had_selection && !k->get_command() && !k->get_alt()) {
                    cursor_set_line(D()->selection.from_line);
                    cursor_set_column(D()->selection.from_column);
                    deselect();
                    break;
#ifdef APPLE_STYLE_KEYS
                } else {
#else
                } else if (!k->get_alt()) {
#endif
                    deselect();

                }
#ifdef APPLE_STYLE_KEYS
                if (k->get_command()) {
                    // Start at first column (it's slightly faster that way) and look for the first non-whitespace
                    // character.
                    int new_cursor_pos = 0;
                    for (int i = 0; i < D()->text[ D()->cursor.line].length(); ++i) {
                        if (!_is_whitespace(D()->text[ D()->cursor.line][i])) {
                            new_cursor_pos = i;
                            break;
                        }
                    }
                    if (new_cursor_pos == D()->cursor.column) {
                        // We're already at the first text character, so move to the very beginning of the line.
                        cursor_set_column(0);
                    } else {
                        // We're somewhere to the right of the first text character; move to the first one.
                        cursor_set_column(new_cursor_pos);
                    }
                } else if (k->get_alt()) {
#else
                if (k->get_alt()) {
                    scancode_handled = false;
                    break;
                } else if (k->get_command()) {
#endif
                    int cc = D()->cursor.column;

                    if (cc == 0 && D()->cursor.line > 0) {
                        cursor_set_line(D()->cursor.line - 1);
                        cursor_set_column(D()->text[ D()->cursor.line].length());
                    } else {
                        bool prev_char = false;

                        while (cc > 0) {
                            bool ischar = _te_is_text_char(D()->text[ D()->cursor.line][cc - 1]);

                            if (prev_char && !ischar)
                                break;

                            prev_char = ischar;
                            cc--;
                        }
                        cursor_set_column(cc);
                    }

                } else if (D()->cursor.column == 0) {

                    if (D()->cursor.line > 0) {
                        cursor_set_line(D()->cursor.line -
                                        num_lines_from(CLAMP<int>(D()->cursor.line - 1, 0, D()->text.size() - 1), -1));
                        cursor_set_column(D()->text[ D()->cursor.line].length());
                    }
                } else {
                    cursor_set_column(cursor_get_column() - 1);
                }

                if (k->get_shift())
                    _post_shift_selection();

            } break;
            case KEY_KP_6: {
                if (k->get_unicode() != 0) {
                    scancode_handled = false;
                    break;
                }
                [[fallthrough]];
            }
            case KEY_RIGHT: {

                if (k->get_shift()) {
                    _pre_shift_selection();
                } else if (had_selection && !k->get_command() && !k->get_alt()) {
                    cursor_set_line(D()->selection.to_line);
                    cursor_set_column(D()->selection.to_column);
                    deselect();
                    break;
#ifdef APPLE_STYLE_KEYS
                } else {
#else
                } else if (!k->get_alt()) {
#endif
                    deselect();
                }

#ifdef APPLE_STYLE_KEYS
                if (k->get_command()) {
                    cursor_set_column(D()->text[ D()->cursor.line].length());
                } else if (k->get_alt()) {
#else
                if (k->get_alt()) {
                    scancode_handled = false;
                    break;
                } else if (k->get_command()) {
#endif
                    int cc = D()->cursor.column;

                    if (cc == D()->text[ D()->cursor.line].length() && D()->cursor.line < D()->text.size() - 1) {
                        cursor_set_line(D()->cursor.line + 1);
                        cursor_set_column(0);
                    } else {
                        bool prev_char = false;

                        while (cc < D()->text[ D()->cursor.line].length()) {
                            bool ischar = _te_is_text_char(D()->text[ D()->cursor.line][cc]);

                            if (prev_char && !ischar)
                                break;
                            prev_char = ischar;
                            cc++;
                        }
                        cursor_set_column(cc);
                    }

                } else if (D()->cursor.column == D()->text[ D()->cursor.line].length()) {

                    if (D()->cursor.line < D()->text.size() - 1) {
                        cursor_set_line(
                                cursor_get_line() +
                                        num_lines_from(CLAMP<int>(D()->cursor.line + 1, 0, D()->text.size() - 1), 1),
                                true, false);
                        cursor_set_column(0);
                    }
                } else {
                    cursor_set_column(cursor_get_column() + 1);
                }

                if (k->get_shift())
                    _post_shift_selection();

            } break;
            case KEY_KP_8: {
                if (k->get_unicode() != 0) {
                    scancode_handled = false;
                    break;
                }
                [[fallthrough]];
            }
            case KEY_UP: {

                if (k->get_alt()) {
                    scancode_handled = false;
                    break;
                }
#ifndef APPLE_STYLE_KEYS
                if (k->get_command()) {
#else
                if (k->get_command() && k->get_alt()) {
#endif
                    _scroll_lines_up();
                    break;
                }

                if (k->get_shift()) {
                    _pre_shift_selection();
                }

#ifdef APPLE_STYLE_KEYS
                if (k->get_command()) {

                    cursor_set_line(0);
                } else
#endif
                {
                    int cur_wrap_index = get_cursor_wrap_index();
                    if (cur_wrap_index > 0) {
                        cursor_set_line(D()->cursor.line, true, false, cur_wrap_index - 1);
                    } else if (D()->cursor.line == 0) {
                        cursor_set_column(0);
                    } else {
                        int new_line = D()->cursor.line - num_lines_from(D()->cursor.line - 1, -1);
                        if (is_line_wrapped(new_line)) {
                            cursor_set_line(new_line, true, false, get_line_wrap_count(new_line));
                        } else {
                            cursor_set_line(new_line, true, false);
                        }
                    }
                }

                if (k->get_shift())
                    _post_shift_selection();
                D()->_cancel_code_hint();

            } break;
            case KEY_KP_2: {
                if (k->get_unicode() != 0) {
                    scancode_handled = false;
                    break;
                }
                [[fallthrough]];
            }
            case KEY_DOWN: {

                if (k->get_alt()) {
                    scancode_handled = false;
                    break;
                }
#ifndef APPLE_STYLE_KEYS
                if (k->get_command()) {
#else
                if (k->get_command() && k->get_alt()) {
#endif
                    _scroll_lines_down();
                    break;
                }

                if (k->get_shift()) {
                    _pre_shift_selection();
                }

#ifdef APPLE_STYLE_KEYS
                if (k->get_command()) {
                    cursor_set_line(get_last_unhidden_line(), true, false, 9999);
                } else
#endif
                {
                    int cur_wrap_index = get_cursor_wrap_index();
                    if (cur_wrap_index < get_line_wrap_count(D()->cursor.line)) {
                        cursor_set_line(D()->cursor.line, true, false, cur_wrap_index + 1);
                    } else if (D()->cursor.line == get_last_unhidden_line()) {
                        cursor_set_column(D()->text[ D()->cursor.line].length());
                    } else {
                        int new_line = D()->cursor.line +
                                       num_lines_from(CLAMP<int>(D()->cursor.line + 1, 0, D()->text.size() - 1), 1);
                        cursor_set_line(new_line, true, false, 0);
                    }
                }

                if (k->get_shift())
                    _post_shift_selection();
                D()->_cancel_code_hint();

            } break;
            case KEY_DELETE: {

                if (readonly)
                    break;

                if (k->get_shift() && !k->get_command() && !k->get_alt() && is_shortcut_keys_enabled()) {
                    cut();
                    break;
                }

                int curline_len = D()->text[ D()->cursor.line].length();

                if (D()->cursor.line == D()->text.size() - 1 && D()->cursor.column == curline_len)
                    break; // Nothing to do.

                int next_line = D()->cursor.column < curline_len ? D()->cursor.line : D()->cursor.line + 1;
                int next_column;

#ifdef APPLE_STYLE_KEYS
                if (k->get_alt() && D()->cursor.column < curline_len - 1) {
#else
                if (k->get_alt()) {
                    scancode_handled = false;
                    break;
                } else if (k->get_command() && D()->cursor.column < curline_len - 1) {
#endif

                    int line = D()->cursor.line;
                    int column = D()->cursor.column;

                    // Check if we are removing a single whitespace, if so remove it and the next char type,
                    // else we just remove the whitespace.
                    bool only_whitespace = false;
                    if (_is_whitespace(D()->text[line][column]) && _is_whitespace(D()->text[line][column + 1])) {
                        only_whitespace = true;
                    } else if (_is_whitespace(D()->text[line][column])) {
                        // Remove the single whitespace.
                        column++;
                    }

                    // Check if its a text char.
                    bool only_char = (_te_is_text_char(D()->text[line][column]) && !only_whitespace);

                    // If its not whitespace or char then symbol.
                    bool only_symbols = !(only_whitespace || only_char);

                    while (column < curline_len) {
                        bool is_whitespace = _is_whitespace(D()->text[line][column]);
                        bool is_text_char = _te_is_text_char(D()->text[line][column]);

                        if (only_whitespace && !is_whitespace) {
                            break;
                        } else if (only_char && !is_text_char) {
                            break;
                        } else if (only_symbols && (is_whitespace || is_text_char)) {
                            break;
                        }
                        column++;
                    }

                    next_line = line;
                    next_column = column;
#ifdef APPLE_STYLE_KEYS
                } else if (k->get_command()) {
                    next_column = curline_len;
                    next_line = D()->cursor.line;
#endif
                } else {
                    next_column = D()->cursor.column < curline_len ? (D()->cursor.column + 1) : 0;
                }

                D()->_remove_text(D()->cursor.line, D()->cursor.column, next_line, next_column);
                update();

            } break;
            case KEY_KP_7: {
                if (k->get_unicode() != 0) {
                    scancode_handled = false;
                    break;
                }
                [[fallthrough]];
            }
            case KEY_HOME: {
#ifdef APPLE_STYLE_KEYS
                if (k->get_shift()) {
                    _pre_shift_selection();
                }

                cursor_set_line(0);

                if (k->get_shift()) {
                    _post_shift_selection();
                } else if (k->get_command() || k->get_control()) {
                    deselect();
                }
#else
                if (k->get_shift())
                    _pre_shift_selection();

                if (k->get_command()) {
                    cursor_set_line(0);
                    cursor_set_column(0);
                } else {

                    // Move cursor column to start of wrapped row and then to start of text.
                    Vector<UIString> rows = get_wrap_rows_text(D()->cursor.line);
                    int wi = get_cursor_wrap_index();
                    int row_start_col = 0;
                    for (int i = 0; i < wi; i++) {
                        row_start_col += rows[i].length();
                    }
                    if (D()->cursor.column == row_start_col || wi == 0) {
                        // Compute whitespace symbols seq length.
                        int current_line_whitespace_len = 0;
                        while (current_line_whitespace_len < D()->text[ D()->cursor.line].length()) {
                            CharType c = D()->text[ D()->cursor.line][current_line_whitespace_len];
                            if (c != '\t' && c != ' ')
                                break;
                            current_line_whitespace_len++;
                        }

                        if (D()->cursor_get_column() == current_line_whitespace_len)
                            cursor_set_column(0);
                        else
                            cursor_set_column(current_line_whitespace_len);
                    } else {
                        cursor_set_column(row_start_col);
                    }
                }

                if (k->get_shift())
                    _post_shift_selection();
                else if (k->get_command() || k->get_control())
                    deselect();
                D()->_cancel_completion();
                D()->completion_hint = "";
#endif
            } break;
            case KEY_KP_1: {
                if (k->get_unicode() != 0) {
                    scancode_handled = false;
                    break;
                }
                [[fallthrough]];
            }
            case KEY_END: {
#ifdef APPLE_STYLE_KEYS
                if (k->get_shift()) {
                    _pre_shift_selection();
                }

                cursor_set_line(get_last_unhidden_line(), true, false, 9999);

                if (k->get_shift()) {
                    _post_shift_selection();
                } else if (k->get_command() || k->get_control()) {
                    deselect();
                }
#else
                if (k->get_shift())
                    _pre_shift_selection();

                if (k->get_command())
                    cursor_set_line(get_last_unhidden_line(), true, false, 9999);

                // Move cursor column to end of wrapped row and then to end of text.
                Vector<UIString> rows = get_wrap_rows_text(D()->cursor.line);
                int wi = get_cursor_wrap_index();
                int row_end_col = -1;
                for (int i = 0; i < wi + 1; i++) {
                    row_end_col += rows[i].length();
                }
                if (wi == rows.size() - 1 || D()->cursor.column == row_end_col) {
                    cursor_set_column(D()->text[ D()->cursor.line].length());
                } else {
                    cursor_set_column(row_end_col);
                }

                if (k->get_shift())
                    _post_shift_selection();
                else if (k->get_command() || k->get_control())
                    deselect();

                D()->_cancel_completion();
                D()->completion_hint = "";
#endif
            } break;
            case KEY_KP_9: {
                if (k->get_unicode() != 0) {
                    scancode_handled = false;
                    break;
                }
                [[fallthrough]];
            }
            case KEY_PAGEUP: {

                if (k->get_shift())
                    _pre_shift_selection();

                int wi;
                int n_line = D()->cursor.line -
                             num_lines_from_rows(D()->cursor.line, get_cursor_wrap_index(), -get_visible_rows(), wi) +
                             1;
                cursor_set_line(n_line, true, false, wi);

                if (k->get_shift())
                    _post_shift_selection();

                D()->_cancel_completion();
                D()->completion_hint = "";

            } break;
            case KEY_KP_3: {
                if (k->get_unicode() != 0) {
                    scancode_handled = false;
                    break;
                }
                [[fallthrough]];
            }
            case KEY_PAGEDOWN: {

                if (k->get_shift())
                    _pre_shift_selection();

                int wi;
                int n_line = D()->cursor.line +
                             num_lines_from_rows(D()->cursor.line, get_cursor_wrap_index(), get_visible_rows(), wi) - 1;
                cursor_set_line(n_line, true, false, wi);

                if (k->get_shift())
                    _post_shift_selection();

                D()->_cancel_completion();
                D()->completion_hint = "";

            } break;
            case KEY_A: {

#ifndef APPLE_STYLE_KEYS
                if (!k->get_control() || k->get_shift() || k->get_alt()) {
                    scancode_handled = false;
                    break;
                }
                if (is_shortcut_keys_enabled()) {
                    select_all();
                }
#else
                if ((!k->get_command() && !k->get_control())) {
                    scancode_handled = false;
                    break;
                }
                if (!k->get_shift() && k->get_command() && is_shortcut_keys_enabled())
                    select_all();
                else if (k->get_control()) {
                    if (k->get_shift())
                        _pre_shift_selection();

                    int current_line_whitespace_len = 0;
                    while (current_line_whitespace_len < D()->text[ D()->cursor.line].length()) {
                        CharType c = D()->text[ D()->cursor.line][current_line_whitespace_len];
                        if (c != '\t' && c != ' ')
                            break;
                        current_line_whitespace_len++;
                    }

                    if (D()->cursor_get_column() == current_line_whitespace_len)
                        cursor_set_column(0);
                    else
                        cursor_set_column(current_line_whitespace_len);

                    if (k->get_shift())
                        _post_shift_selection();
                    else if (k->get_command() || k->get_control())
                        deselect();
                }
            } break;
            case KEY_E: {

                if (!k->get_control() || k->get_command() || k->get_alt()) {
                    scancode_handled = false;
                    break;
                }

                if (k->get_shift())
                    _pre_shift_selection();

                if (k->get_command())
                    cursor_set_line(D()->text.size() - 1, true, false);
                cursor_set_column(D()->text[ D()->cursor.line].length());

                if (k->get_shift())
                    _post_shift_selection();
                else if (k->get_command() || k->get_control())
                    deselect();

                D()->_cancel_completion();
                D()->completion_hint = "";
#endif
            } break;
            case KEY_X: {
                if (readonly) {
                    break;
                }
                if (!k->get_command() || k->get_shift() || k->get_alt()) {
                    scancode_handled = false;
                    break;
                }

                if (is_shortcut_keys_enabled()) {
                cut();
                }

            } break;
            case KEY_C: {

                if (!k->get_command() || k->get_shift() || k->get_alt()) {
                    scancode_handled = false;
                    break;
                }

                if (is_shortcut_keys_enabled()) {
                copy();
                }

            } break;
            case KEY_Z: {

                if (readonly) {
                    break;
                }
                if (!k->get_command()) {
                    scancode_handled = false;
                    break;
                }

                if (is_shortcut_keys_enabled()) {
                if (k->get_shift())
                    redo();
                else
                    undo();
                }
            } break;
            case KEY_Y: {
                if (readonly) {
                    break;
                }

                if (!k->get_command()) {
                    scancode_handled = false;
                    break;
                }

                if (is_shortcut_keys_enabled()) {
                redo();
                }
            } break;
            case KEY_V: {
                if (readonly) {
                    break;
                }
                if (!k->get_command() || k->get_shift() || k->get_alt()) {
                    scancode_handled = false;
                    break;
                }

                if (is_shortcut_keys_enabled()) {
                paste();
                }

            } break;
            case KEY_SPACE: {
#ifdef OSX_ENABLED
                if (D()->completion_enabled && k->get_metakey()) { // cmd-space is spotlight shortcut in OSX
#else
                if (D()->completion_enabled && k->get_command()) {
#endif

                    query_code_comple();
                    scancode_handled = true;
                } else {
                    scancode_handled = false;
                }

            } break;

            case KEY_MENU: {
                if (context_menu_enabled) {
                    D()->popup_show = true;
                    if (!readonly) {
                        menu->set_item_disabled(menu->get_item_index(MENU_UNDO), !has_undo());
                        menu->set_item_disabled(menu->get_item_index(MENU_REDO), !has_redo());
                    }
                    menu->set_position(get_global_transform().xform(_get_cursor_pixel_pos()));
                    menu->set_size(Vector2(1, 1));
                    menu->set_scale(get_global_transform().get_scale());
                    menu->popup();
                    menu->grab_focus();
                }
            } break;
            default: {

                scancode_handled = false;
            } break;
        }

        if (scancode_handled)
            accept_event();

        if (k->get_keycode() == KEY_INSERT) {
            set_insert_mode(!insert_mode);
            accept_event();
            return;
        }

        if (!scancode_handled && !k->get_command()) { // For German keyboards.

            if (k->get_unicode() >= 32) {

                if (readonly)
                    return;

                // Remove the old character if in insert mode and no D()->selection.
                if (insert_mode && !had_selection) {
                    begin_complex_operation();

                    // Make sure we don't try and remove empty space.
                    if (D()->cursor.column < get_line(D()->cursor.line).length()) {
                        D()->_remove_text(
                                D()->cursor.line, D()->cursor.column, D()->cursor.line, D()->cursor.column + 1);
                    }
                }

                const CharType chr = (CharType)k->get_unicode();

                if (!D()->completion_hint.empty() && k->get_unicode() == ')') {
                    D()->completion_hint = "";
                }
                if (auto_brace_completion_enabled && _is_pair_symbol(chr)) {
                    D()->_consume_pair_symbol(chr);
                } else {
                    D()->_insert_text_at_cursor(UIString(chr));
                }

                if (insert_mode && !had_selection) {
                    D()->end_complex_operation();
                }

                if (D()->selection.active != had_selection) {
                    D()->end_complex_operation();
                }
                accept_event();
            }
        }

        return;
    }
}

void TextEdit::_scroll_up(real_t p_delta) {

    if (scrolling && smooth_scroll_enabled && SGN(target_v_scroll - v_scroll->get_value()) != SGN(-p_delta)) {
        scrolling = false;
        minimap_clicked = false;
    }

    if (scrolling) {
        target_v_scroll = (target_v_scroll - p_delta);
    } else {
        target_v_scroll = (get_v_scroll() - p_delta);
    }

    if (smooth_scroll_enabled) {
        if (target_v_scroll <= 0) {
            target_v_scroll = 0;
        }
        if (Math::abs(target_v_scroll - v_scroll->get_value()) < 1.0f) {
            v_scroll->set_value(target_v_scroll);
        } else {
            scrolling = true;
            set_physics_process_internal(true);
        }
    } else {
        set_v_scroll(target_v_scroll);
    }
}

void TextEdit::_scroll_down(real_t p_delta) {

    if (scrolling && smooth_scroll_enabled && SGN(target_v_scroll - v_scroll->get_value()) != SGN(p_delta)) {
        scrolling = false;
        minimap_clicked = false;
    }

    if (scrolling) {
        target_v_scroll = (target_v_scroll + p_delta);
    } else {
        target_v_scroll = (get_v_scroll() + p_delta);
    }

    if (smooth_scroll_enabled) {
        int max_v_scroll = round(v_scroll->get_max() - v_scroll->get_page());
        if (target_v_scroll > max_v_scroll) {
            target_v_scroll = max_v_scroll;
        }
        if (Math::abs(target_v_scroll - v_scroll->get_value()) < 1.0f) {
            v_scroll->set_value(target_v_scroll);
        } else {
            scrolling = true;
            set_physics_process_internal(true);
        }
    } else {
        set_v_scroll(target_v_scroll);
    }
}

void TextEdit::_pre_shift_selection() {
    using Selection=PrivateData::Selection;
    if (!D()->selection.active || D()->selection.selecting_mode == Selection::MODE_NONE) {

        D()->selection.selecting_line = D()->cursor.line;
        D()->selection.selecting_column = D()->cursor.column;
        D()->selection.active = true;
    }

    D()->selection.selecting_mode = Selection::MODE_SHIFT;
}

void TextEdit::_post_shift_selection() {
    using Selection=PrivateData::Selection;

    if (D()->selection.active && D()->selection.selecting_mode == Selection::MODE_SHIFT) {

        select(D()->selection.selecting_line, D()->selection.selecting_column, D()->cursor.line, D()->cursor.column);
        update();
    }

    D()->selection.selecting_text = true;
}

void TextEdit::_scroll_lines_up() {
    scrolling = false;
    minimap_clicked = false;

    // Adjust the vertical scroll.
    set_v_scroll(get_v_scroll() - 1);

    // Adjust the cursor to viewport.
    if (!D()->selection.active) {
        int cur_line = D()->cursor.line;
        int cur_wrap = get_cursor_wrap_index();
        int last_vis_line = get_last_full_visible_line();
        int last_vis_wrap = get_last_full_visible_line_wrap_index();

        if (cur_line > last_vis_line || (cur_line == last_vis_line && cur_wrap > last_vis_wrap)) {
            cursor_set_line(last_vis_line, false, false, last_vis_wrap);
        }
    }
}

void TextEdit::_scroll_lines_down() {
    scrolling = false;
    minimap_clicked = false;

    // Adjust the vertical scroll.
    set_v_scroll(get_v_scroll() + 1);

    // Adjust the cursor to viewport.
    if (!D()->selection.active) {
        int cur_line = D()->cursor.line;
        int cur_wrap = get_cursor_wrap_index();
        int first_vis_line = get_first_visible_line();
        int first_vis_wrap = D()->cursor.wrap_ofs;

        if (cur_line < first_vis_line || (cur_line == first_vis_line && cur_wrap < first_vis_wrap)) {
            cursor_set_line(first_vis_line, false, false, first_vis_wrap);
        }
    }
}

/**** TEXT EDIT CORE API ****/


int TextEdit::get_char_count() {

    return D()->get_char_count();
}

Size2 TextEdit::get_minimum_size() const {

    return D()->cache.style_normal->get_minimum_size();
}

int TextEdit::_get_control_height() const {
    int control_height = get_size().height;
    control_height -= D()->cache.style_normal->get_minimum_size().height;
    if (h_scroll->is_visible_in_tree()) {
        control_height -= h_scroll->get_size().height;
    }
    return control_height;
}

void TextEdit::_generate_context_menu() {
    // Reorganize context menu.
    menu->clear();
    if (!readonly)
        menu->add_item(RTR("Cut"), MENU_CUT, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_X : 0);
    menu->add_item(RTR("Copy"), MENU_COPY, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_C : 0);
    if (!readonly)
        menu->add_item(RTR("Paste"), MENU_PASTE, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_V : 0);
    menu->add_separator();
    if (D()->selecting_enabled || !readonly) {
        menu->add_separator();
    }
    if (D()->selecting_enabled) {
        menu->add_item(RTR("Select All"), MENU_SELECT_ALL, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_A : 0);
    }
    if (!readonly) {
        menu->add_item(RTR("Clear"), MENU_CLEAR);
        menu->add_separator();
        menu->add_item(RTR("Undo"), MENU_UNDO, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_Z : 0);
        menu->add_item(RTR("Redo"), MENU_REDO, is_shortcut_keys_enabled() ? KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_Z : 0);
    }
}

int TextEdit::get_visible_rows() const {

    return _get_control_height() / get_row_height();
}

int TextEdit::_get_minimap_visible_rows() const {
    return _get_control_height() / (minimap_char_size.y + minimap_line_spacing);
}
int TextEdit::get_total_visible_rows() const {

    // Returns the total amount of rows we need in the editor.
    // This skips hidden lines and counts each wrapping of a line.
    if (!is_hiding_enabled() && !is_wrap_enabled())
        return D()->text.size();

    int total_rows = 0;
    for (size_t i = 0; i < D()->text.size(); i++) {
        if (!D()->text.is_hidden(i)) {
            total_rows++;
            total_rows += get_line_wrap_count(i);
        }
    }
    return total_rows;
}

void TextEdit::_update_wrap_at() {

    D()->wrap_at = get_size().width - D()->cache.style_normal->get_minimum_size().width - D()->cache.line_number_w -
                   D()->cache.breakpoint_gutter_width - D()->cache.fold_gutter_width - D()->cache.info_gutter_width -
                   D()->cache.minimap_width - D()->wrap_right_offset;
    update_cursor_wrap_offset();
    D()->text.clear_wrap_cache();

    for (int i = 0; i < D()->text.size(); i++) {
        // Update all values that wrap.
        if (!is_line_wrapped(i))
            continue;
        Vector<UIString> rows = get_wrap_rows_text(i);
        D()->text.set_line_wrap_amount(i, rows.size() - 1);
    }
}

void TextEdit::adjust_viewport_to_cursor() {

    // Make sure cursor is visible on the screen.
    scrolling = false;
    minimap_clicked = false;

    int cur_line = D()->cursor.line;
    int cur_wrap = get_cursor_wrap_index();

    int first_vis_line = get_first_visible_line();
    int first_vis_wrap = D()->cursor.wrap_ofs;
    int last_vis_line = get_last_full_visible_line();
    int last_vis_wrap = get_last_full_visible_line_wrap_index();

    if (cur_line < first_vis_line || (cur_line == first_vis_line && cur_wrap < first_vis_wrap)) {
        // Cursor is above screen.
        set_line_as_first_visible(cur_line, cur_wrap);
    } else if (cur_line > last_vis_line || (cur_line == last_vis_line && cur_wrap > last_vis_wrap)) {
        // Cursor is below screen.
        set_line_as_last_visible(cur_line, cur_wrap);
    }

    int visible_width = get_size().width - D()->cache.style_normal->get_minimum_size().width -
                        D()->cache.line_number_w - D()->cache.breakpoint_gutter_width - D()->cache.fold_gutter_width -
                        D()->cache.info_gutter_width - D()->cache.minimap_width;
    if (v_scroll->is_visible_in_tree())
        visible_width -= v_scroll->get_combined_minimum_size().width;
    visible_width -= 20; // Give it a little more space.

    if (!is_wrap_enabled()) {
        // Adjust x offset.
        int cursor_x = get_column_x_offset(D()->cursor.column, D()->text[D()->cursor.line]);

        if (cursor_x > (D()->cursor.x_ofs + visible_width))
            D()->cursor.x_ofs = cursor_x - visible_width + 1;

        if (cursor_x < D()->cursor.x_ofs)
            D()->cursor.x_ofs = cursor_x;
    } else {
        D()->cursor.x_ofs = 0;
    }
    h_scroll->set_value(D()->cursor.x_ofs);

    update();
}

void TextEdit::center_viewport_to_cursor() {

    // Move viewport so the cursor is in the center of the screen.
    scrolling = false;
    minimap_clicked = false;

    if (is_line_hidden(D()->cursor.line))
        unfold_line(D()->cursor.line);

    set_line_as_center_visible(D()->cursor.line, get_cursor_wrap_index());
    int visible_width = get_size().width - D()->cache.style_normal->get_minimum_size().width -
                        D()->cache.line_number_w - D()->cache.breakpoint_gutter_width - D()->cache.fold_gutter_width -
                        D()->cache.info_gutter_width - D()->cache.minimap_width;
    if (v_scroll->is_visible_in_tree())
        visible_width -= v_scroll->get_combined_minimum_size().width;
    visible_width -= 20; // Give it a little more space.

    if (is_wrap_enabled()) {
        // Center x offset.
        int cursor_x = get_column_x_offset_for_line(D()->cursor.column, D()->cursor.line);

        if (cursor_x > (D()->cursor.x_ofs + visible_width))
            D()->cursor.x_ofs = cursor_x - visible_width + 1;

        if (cursor_x < D()->cursor.x_ofs)
            D()->cursor.x_ofs = cursor_x;
    } else {
        D()->cursor.x_ofs = 0;
    }
    h_scroll->set_value(D()->cursor.x_ofs);

    update();
}

void TextEdit::update_cursor_wrap_offset() {
    int first_vis_line = get_first_visible_line();
    if (is_line_wrapped(first_vis_line)) {
        D()->cursor.wrap_ofs = MIN(D()->cursor.wrap_ofs, get_line_wrap_count(first_vis_line));
    } else {
        D()->cursor.wrap_ofs = 0;
    }
    set_line_as_first_visible(D()->cursor.line_ofs, D()->cursor.wrap_ofs);
}

bool TextEdit::is_line_wrapped(int line) const {

    ERR_FAIL_INDEX_V(line, D()->text.size(), 0);
    if (!is_wrap_enabled())
        return false;
    return D()->text.get_line_width(line) > D()->wrap_at;
}

int TextEdit::get_line_wrap_count(int line) const {

    ERR_FAIL_INDEX_V(line, D()->text.size(), 0);
    if (!is_line_wrapped(line))
        return 0;

    int wrap_amount = D()->text.get_line_wrap_amount(line);
    if (wrap_amount == -1) {
        // Update the value.
        Vector<UIString> rows = get_wrap_rows_text(line);
        wrap_amount = rows.size() - 1;
        D()->text.set_line_wrap_amount(line, wrap_amount);
    }

    return wrap_amount;
}

Vector<UIString> TextEdit::get_wrap_rows_text(int p_line) const {

    ERR_FAIL_INDEX_V(p_line, D()->text.size(), Vector<UIString>());

    Vector<UIString> lines;
    if (!is_line_wrapped(p_line)) {
        lines.emplace_back(D()->text[p_line]);
        return lines;
    }

    int px = 0;
    int col = 0;
    UIString line_text = D()->text[p_line];
    UIString wrap_substring;

    int word_px = 0;
    UIString word_str;
    int cur_wrap_index = 0;

    int tab_offset_px = get_indent_level(p_line) * D()->cache.font->get_char_size(' ').width;
    if (tab_offset_px >= D()->wrap_at) {
        tab_offset_px = 0;
    }

    while (col < line_text.length()) {
        CharType c = line_text[col];
        CharType next_char = ((col+1)<line_text.size()) ? line_text[col] : CharType(0);
        int w = D()->text.get_char_width(c, next_char, px + word_px);

        int indent_ofs = (cur_wrap_index != 0 ? tab_offset_px : 0);

        if (indent_ofs + word_px + w > D()->wrap_at) {
            // Not enough space to add this char; start next line.
            wrap_substring += word_str;
            lines.push_back(wrap_substring);
            cur_wrap_index++;
            wrap_substring.clear();
            px = 0;

            word_str = c; // reset word_str to c
            word_px = w;
        } else {
            word_str += c;
            word_px += w;
            if (c == ' ') {
                // End of a word; add this word to the substring.
                wrap_substring += word_str;
                px += word_px;
                word_str.clear();
                word_px = 0;
            }

            if (indent_ofs + px + word_px > D()->wrap_at) {
                // This word will be moved to the next line.
                lines.push_back(wrap_substring);
                // Reset for next wrap.
                cur_wrap_index++;
                wrap_substring.clear();
                px = 0;
            }
        }
        col++;
    }
    // Line ends before hit wrap_at; add this word to the substring.
    wrap_substring += word_str;
    lines.push_back(wrap_substring);

    // Update cache.
    D()->text.set_line_wrap_amount(p_line, lines.size() - 1);

    return lines;
}

int TextEdit::get_cursor_wrap_index() const {

    return get_line_wrap_index_at_col(D()->cursor.line, D()->cursor.column);
}

int TextEdit::get_line_wrap_index_at_col(int p_line, int p_column) const {

    ERR_FAIL_INDEX_V(p_line, D()->text.size(), 0);

    if (!is_line_wrapped(p_line))
        return 0;

    // Loop through wraps in the line text until we get to the column.
    int wrap_index = 0;
    int col = 0;
    Vector<UIString> rows = get_wrap_rows_text(p_line);
    for (int i = 0; i < rows.size(); i++) {
        wrap_index = i;
        UIString s = rows[wrap_index];
        col += s.length();
        if (col > p_column)
            break;
    }
    return wrap_index;
}

void TextEdit::cursor_set_column(int p_col, bool p_adjust_viewport) {

    if (p_col < 0)
        p_col = 0;

    D()->cursor.column = p_col;
    size_t line_length = get_line(D()->cursor.line).length();
    if (D()->cursor.column > line_length)
        D()->cursor.column = line_length;

    D()->cursor.last_fit_x = get_column_x_offset_for_line(D()->cursor.column, D()->cursor.line);

    if (p_adjust_viewport)
        adjust_viewport_to_cursor();

    if (!D()->cursor_changed_dirty) {
        if (is_inside_tree()) {
            call_deferred([this] () {_cursor_changed_emit();});
        }
        D()->cursor_changed_dirty = true;
    }
}

void TextEdit::cursor_set_line(int p_row, bool p_adjust_viewport, bool p_can_be_hidden, int p_wrap_index) {

    D()->cursor_set_line(p_row, p_adjust_viewport, p_can_be_hidden, p_wrap_index);
}

int TextEdit::cursor_get_column() const {

    return D()->cursor_get_column();
}

int TextEdit::cursor_get_line() const {

    return D()->cursor_get_line();
}

bool TextEdit::cursor_get_blink_enabled() const {
    return caret_blink_enabled;
}

void TextEdit::cursor_set_blink_enabled(const bool p_enabled) {
    caret_blink_enabled = p_enabled;

    if (has_focus()) {
        if (p_enabled) {
            caret_blink_timer->start();
        } else {
            caret_blink_timer->stop();
        }
    }
    draw_caret = true;
}

float TextEdit::cursor_get_blink_speed() const {
    return caret_blink_timer->get_wait_time();
}

void TextEdit::cursor_set_blink_speed(const float p_speed) {
    ERR_FAIL_COND(p_speed <= 0);
    caret_blink_timer->set_wait_time(p_speed);
}

void TextEdit::cursor_set_block_mode(const bool p_enable) {
    block_caret = p_enable;
    update();
}

bool TextEdit::cursor_is_block_mode() const {
    return block_caret;
}

void TextEdit::set_right_click_moves_caret(bool p_enable) {
    right_click_moves_caret = p_enable;
}

bool TextEdit::is_right_click_moving_caret() const {
    return right_click_moves_caret;
}

void TextEdit::_v_scroll_input() {
    scrolling = false;
    minimap_clicked = false;
}

void TextEdit::_scroll_moved(double p_to_val) {

    if (updating_scrolls)
        return;

    if (h_scroll->is_visible_in_tree())
        D()->cursor.x_ofs = h_scroll->get_value();
    if (v_scroll->is_visible_in_tree()) {

        // Set line ofs and wrap ofs.
        int v_scroll_i = floor(get_v_scroll());
        int sc = 0;
        int n_line;
        for (n_line = 0; n_line < D()->text.size(); n_line++) {
            if (!is_line_hidden(n_line)) {
                sc++;
                sc += get_line_wrap_count(n_line);
                if (sc > v_scroll_i)
                    break;
            }
        }
        n_line = MIN(n_line, D()->text.size() - 1);
        int line_wrap_amount = get_line_wrap_count(n_line);
        int wi = line_wrap_amount - (sc - v_scroll_i - 1);
        wi = CLAMP(wi, 0, line_wrap_amount);

        D()->cursor.line_ofs = n_line;
        D()->cursor.wrap_ofs = wi;
    }
    update();
}

int TextEdit::get_row_height() const {

    return D()->cache.font->get_height() + D()->cache.line_spacing;
}

/* Line and character position. */
Point2 TextEdit::get_pos_at_line_column(int p_line, int p_column) const {
    Rect2i rect = get_rect_at_line_column(p_line, p_column);
    return rect.position + Vector2i(0, get_line_height());
}

Rect2 TextEdit::get_rect_at_line_column(int p_line, int p_column) const {
    ERR_FAIL_INDEX_V(p_line, D()->text.size(), Rect2i(-1, -1, 0, 0));
    ERR_FAIL_COND_V(p_column < 0, Rect2i(-1, -1, 0, 0));
    ERR_FAIL_COND_V(p_column > D()->text[p_line].length(), Rect2i(-1, -1, 0, 0));

    if (D()->line_drawing_cache.size() == 0 || !D()->line_drawing_cache.contains(p_line)) {
        // Line is not in the cache, which means it's outside of the viewing area.
        return Rect2i(-1, -1, 0, 0);
    }
    auto cache_entry = D()->line_drawing_cache[p_line];

    int wrap_index = get_line_wrap_index_at_col(p_line, p_column);
    if (wrap_index >= cache_entry.first_visible_char.size()) {
        // Line seems to be wrapped beyond the viewable area.
        return Rect2i(-1, -1, 0, 0);
    }

    int first_visible_char = cache_entry.first_visible_char[wrap_index];
    int last_visible_char = cache_entry.last_visible_char[wrap_index];
    if (p_column < first_visible_char || p_column > last_visible_char) {
        // Character is outside of the viewing area, no point calculating its position.
        return Rect2i(-1, -1, 0, 0);
    }

    Point2i pos, size;
    pos.y = cache_entry.y_offset + get_line_height() * wrap_index;
    pos.x = get_total_gutter_width() + D()->cache.style_normal->get_margin(Margin::Left) - get_h_scroll();

    int start_x = get_column_x_offset_for_line(p_column, p_line);
    pos.x += start_x;

    UIString line = D()->text[p_line];
    size.x = D()->cache.font->get_char_size(line[p_column]).width;
    size.y = get_line_height();

    return Rect2i(pos, size);
}

Point2 TextEdit::get_line_column_at_pos(const Point2 &p_pos) const {
    int row, col;
    _get_mouse_pos(p_pos, row, col);

    return Point2i(col, row);
}
int TextEdit::get_char_pos_for_line(int p_px, int p_line, int p_wrap_index) const {

    ERR_FAIL_INDEX_V(p_line, D()->text.size(), 0);

    if (is_line_wrapped(p_line)) {
        int line_wrap_amount = get_line_wrap_count(p_line);
        int wrap_offset_px = get_indent_level(p_line) * D()->cache.font->get_char_size(' ').width;
        if (wrap_offset_px >= D()->wrap_at) {
            wrap_offset_px = 0;
        }
        if (p_wrap_index > line_wrap_amount)
            p_wrap_index = line_wrap_amount;
        if (p_wrap_index > 0)
            p_px -= wrap_offset_px;
        else
            p_wrap_index = 0;
        Vector<UIString> rows = get_wrap_rows_text(p_line);
        int c_pos = get_char_pos_for(p_px, rows[p_wrap_index]);
        for (int i = 0; i < p_wrap_index; i++) {
            UIString s = rows[i];
            c_pos += s.length();
        }

        return c_pos;
    } else {

        return get_char_pos_for(p_px, D()->text[p_line]);
    }
}

int TextEdit::get_column_x_offset_for_line(int p_char, int p_line) const {

    ERR_FAIL_INDEX_V(p_line, D()->text.size(), 0);

    if (!is_line_wrapped(p_line)) {

        return get_column_x_offset(p_char, D()->text[p_line]);
    }

    int n_char = p_char;
    int col = 0;
    Vector<UIString> rows = get_wrap_rows_text(p_line);
    int wrap_index = 0;
    for (int i = 0; i < rows.size(); i++) {
        wrap_index = i;
        UIString s = rows[wrap_index];
        col += s.length();
        if (col > p_char)
            break;
        n_char -= s.length();
    }
    int px = get_column_x_offset(n_char, rows[wrap_index]);

    int wrap_offset_px = get_indent_level(p_line) * D()->cache.font->get_char_size(' ').width;
    if (wrap_offset_px >= D()->wrap_at) {
        wrap_offset_px = 0;
    }
    if (wrap_index != 0)
        px += wrap_offset_px;

    return px;
}

int TextEdit::get_char_pos_for(int p_px, const UIString& p_str) const {

    int px = 0;
    int c = 0;
    auto len = p_str.length();
    while (c < len) {

        CharType next = (c+1)<len ? p_str[c + 1] : CharType(0);
        int w = D()->text.get_char_width(p_str[c], next, px);

        if (p_px < (px + w / 2))
            break;
        px += w;
        c++;
    }

    return c;
}

int TextEdit::get_column_x_offset(int p_char, const UIString& p_str) const {

    int px = 0;

    auto len = p_str.length();
    for (int i = 0; i < len; i++) {

        if (i >= p_char)
            break;
        CharType next = ((i+1)>=len) ? CharType(0) : p_str[i + 1];
        px += D()->text.get_char_width(p_str[i], next, px);
    }

    return px;
}

void TextEdit::insert_text_at_cursor_ui(const UIString &p_text) {

    D()->insert_text_at_cursor(p_text);
}
void TextEdit::insert_text_at_cursor(StringView _text) {
    insert_text_at_cursor_ui(StringUtils::from_utf8(_text));
}
Variant TextEdit::get_drag_data(const Point2 &p_point) {
    if (D()->selection.active && D()->selection.drag_attempt) {
        String t = get_selection_text();
        Label *l = memnew(Label);
        l->set_text(t);
        set_drag_preview(l);
        return t;
    }

    return Variant();
}

bool TextEdit::can_drop_data(const Point2 &p_point, const Variant &p_data) const {
    bool drop_override = Control::can_drop_data(p_point, p_data); // In case user wants to drop custom data.
    if (drop_override) {
        return drop_override;
    }

    return !readonly && p_data.get_type() == VariantType::STRING;
}

void TextEdit::drop_data(const Point2 &p_point, const Variant &p_data) {
    Control::drop_data(p_point, p_data);

    if (p_data.get_type() == VariantType::STRING && !readonly) {
        Point2 mp = get_local_mouse_position();
        int caret_row_tmp, caret_column_tmp;
        _get_mouse_pos(Point2i(mp.x, mp.y), caret_row_tmp, caret_column_tmp);
        if (D()->selection.drag_attempt) {
            D()->selection.drag_attempt = false;
            if (!is_mouse_over_selection(!Input::get_singleton()->is_key_pressed(KEY_CONTROL))) {
                begin_complex_operation();
                if (!Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
                    if (caret_row_tmp > D()->selection.to_line) {
                        caret_row_tmp = caret_row_tmp - (D()->selection.to_line - D()->selection.from_line);
                    } else if (caret_row_tmp == D()->selection.to_line && caret_column_tmp >= D()->selection.to_column) {
                        caret_column_tmp = caret_column_tmp - (D()->selection.to_column - D()->selection.from_column);
                    }

                    D()->_remove_text(D()->selection.from_line, D()->selection.from_column, D()->selection.to_line, D()->selection.to_column);
                    cursor_set_line(D()->selection.from_line, false);
                    cursor_set_column(D()->selection.from_column);
                    D()->selection.active = false;
                    D()->selection.selecting_mode = PrivateData::Selection::MODE_NONE;
                } else {
                    deselect();
                }

                cursor_set_line(caret_row_tmp, true, false);
                cursor_set_column(caret_column_tmp);
                insert_text_at_cursor(p_data.as<String>());
                end_complex_operation();
            }
        } else if (is_mouse_over_selection()) {
            begin_complex_operation();
            caret_row_tmp = D()->selection.from_line;
            caret_column_tmp = D()->selection.from_column;

            D()->_remove_text(D()->selection.from_line, D()->selection.from_column, D()->selection.to_line, D()->selection.to_column);
            cursor_set_line(D()->selection.from_line, false);
            cursor_set_column(D()->selection.from_column);
            D()->selection.active = false;
            D()->selection.selecting_mode = PrivateData::Selection::MODE_NONE;

            cursor_set_line(caret_row_tmp, true, false);
            cursor_set_column(caret_column_tmp);
            insert_text_at_cursor(p_data.as<String>());
            end_complex_operation();
            grab_focus();
        } else {
            deselect();
            cursor_set_line(caret_row_tmp, true, false);
            cursor_set_column(caret_column_tmp);
            insert_text_at_cursor(p_data.as<String>());
            grab_focus();
        }

        if (caret_row_tmp != D()->cursor.line || caret_column_tmp != D()->cursor.column) {
            select(caret_row_tmp, caret_column_tmp, D()->cursor.line, D()->cursor.column);
        }
    }
}

bool TextEdit::is_mouse_over_selection(bool p_edges) const {
    if (!D()->selection.active) {
        return false;
    }
    Point2 mp = get_local_mouse_position();
    int row, col;
    _get_mouse_pos(Point2i(mp.x, mp.y), row, col);
    if (p_edges) {
        if ((row == D()->selection.from_line && col == D()->selection.from_column) ||
                (row == D()->selection.to_line && col == D()->selection.to_column)) {
            return true;
        }
    }
    return (row >= D()->selection.from_line && row <= D()->selection.to_line &&
            (row > D()->selection.from_line || col > D()->selection.from_column) &&
            (row < D()->selection.to_line || col < D()->selection.to_column));
}
Control::CursorShape TextEdit::get_cursor_shape(const Point2 &p_pos) const {
    if (!D()->highlighted_word.isEmpty()) {
        return CURSOR_POINTING_HAND;
    }

    if ((D()->completion_active && D()->completion_rect.has_point(p_pos)) ||
            (is_readonly() && (!is_selecting_enabled() || D()->text.size() == 0))) {
        return CURSOR_ARROW;
    }
    int gutter = D()->cache.style_normal->get_margin(Margin::Left) + D()->cache.line_number_w +
                 D()->cache.breakpoint_gutter_width + D()->cache.fold_gutter_width + D()->cache.info_gutter_width;
    if (p_pos.x < gutter) {

        int row, col;
        _get_mouse_pos(p_pos, row, col);
        int left_margin = D()->cache.style_normal->get_margin(Margin::Left);

        // Breakpoint icon.
        if (draw_breakpoint_gutter && p_pos.x > left_margin - 6 &&
                p_pos.x <= left_margin + D()->cache.breakpoint_gutter_width - 3) {
            return CURSOR_POINTING_HAND;
        }

        // Info icons.
        int gutter_left = left_margin + D()->cache.breakpoint_gutter_width + D()->cache.info_gutter_width;
        if (draw_info_gutter && p_pos.x > left_margin + D()->cache.breakpoint_gutter_width - 6 &&
                p_pos.x <= gutter_left - 3) {
            if (D()->text.has_info_icon(row)) {
                return CURSOR_POINTING_HAND;
            }
            return CURSOR_ARROW;
        }

        // Fold icon.
        if (draw_fold_gutter && p_pos.x > gutter_left + D()->cache.line_number_w - 6 &&
                p_pos.x <= gutter_left + D()->cache.line_number_w + D()->cache.fold_gutter_width - 3) {
            if (is_folded(row) || can_fold(row))
                return CURSOR_POINTING_HAND;
            else
                return CURSOR_ARROW;
        }

        return CURSOR_ARROW;
    } else {
        int xmargin_end = get_size().width - D()->cache.style_normal->get_margin(Margin::Right);
        if (draw_minimap && p_pos.x > xmargin_end - minimap_width && p_pos.x <= xmargin_end) {
            return CURSOR_ARROW;
        }
        int row, col;
        _get_mouse_pos(p_pos, row, col);
        // EOL fold icon.
        if (is_folded(row)) {
            int line_width = D()->text.get_line_width(row);
            line_width += D()->cache.style_normal->get_margin(Margin::Left) + D()->cache.line_number_w +
                          D()->cache.breakpoint_gutter_width + D()->cache.fold_gutter_width +
                          D()->cache.info_gutter_width - D()->cursor.x_ofs;
            if (p_pos.x > line_width - 3 && p_pos.x <= line_width + D()->cache.folded_eol_icon->get_width() + 3) {
                return CURSOR_POINTING_HAND;
            }
        }
    }

    return get_default_cursor_shape();
}

void TextEdit::set_text_ui(const UIString& p_text) {

    D()->set_text(p_text);
}

String TextEdit::get_text() {
    UIString longthing;
    int len = D()->text.size();
    for (int i = 0; i < len; i++) {

        longthing += D()->text[i];
        if (i != len - 1)
            longthing += UIString("\n");
    }

    return StringUtils::to_utf8(longthing);
}

String TextEdit::get_text_for_lookup_completion() {

    int row, col;
    _get_mouse_pos(get_local_mouse_position(), row, col);

    String longthing;
    int len = D()->text.size();
    for (int i = 0; i < len; i++) {
        String line=StringUtils::to_utf8(D()->text[i]).data();

        if (i == row) {
            longthing += StringUtils::substr(line,0, col);
            longthing.push_back(c_cursor_marker); // Not unicode, represents the D()->cursor.
            longthing += StringUtils::substr(line,col, D()->text[i].size());
        } else {

            longthing += line;
        }

        if (i != len - 1)
            longthing += "\n";
    }

    return longthing;
}

UIString TextEdit::get_text_for_completion() {

    UIString longthing;
    int len = D()->text.size();
    for (int i = 0; i < len; i++) {

        if (i == D()->cursor.line) {
            longthing += StringUtils::substr(D()->text[i],0, D()->cursor.column);
            longthing += UIString(0xFFFF); // Not unicode, represents the D()->cursor.
            longthing += StringUtils::substr(D()->text[i],D()->cursor.column, D()->text[i].size());
        } else {

            longthing += D()->text[i];
        }

        if (i != len - 1)
            longthing += UIString("\n");
    }

    return longthing;
}
String TextEdit::get_text_for_completion_utf8() const {

    String longthing;
    int len = D()->text.size();
    uint8_t marker[2] = {0xFF,0xFF};
    for (int i = 0; i < len; i++) {

        if (i == D()->cursor.line) {
            longthing += StringUtils::to_utf8(StringUtils::substr(D()->text[i],0, D()->cursor.column)).data();
            longthing.append((char *)marker,2); // Not unicode, represents the D()->cursor.
            longthing +=
                    StringUtils::to_utf8(StringUtils::substr(D()->text[i], D()->cursor.column, D()->text[i].size()))
                            .data();
        } else {

            longthing += StringUtils::to_utf8(D()->text[i]).data();
        }

        if (i != len - 1)
            longthing += "\n";
    }

    return longthing;
}

//String TextEdit::get_line(int line) const {

//    if (line < 0 || line >= D()->text.size())
//        return String();

//    return D()->text[line];
//};
String TextEdit::get_line(int line) const {
    return D()->get_line(line);
}
void TextEdit::_clear() {

    D()->_clear();
}

void TextEdit::clear() {

    D()->setting_text = true;
    _clear();
    D()->setting_text = false;
}

void TextEdit::set_readonly(bool p_readonly) {

    if (readonly == p_readonly)
        return;

    readonly = p_readonly;
    _generate_context_menu();

    // Reorganize context menu.
    menu->clear();

    if (!readonly) {
        menu->add_item(RTR("Undo"), MENU_UNDO, KEY_MASK_CMD | KEY_Z);
        menu->add_item(RTR("Redo"), MENU_REDO, KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_Z);
    }

    if (!readonly) {
        menu->add_separator();
        menu->add_item(RTR("Cut"), MENU_CUT, KEY_MASK_CMD | KEY_X);
    }

    menu->add_item(RTR("Copy"), MENU_COPY, KEY_MASK_CMD | KEY_C);

    if (!readonly) {
        menu->add_item(RTR("Paste"), MENU_PASTE, KEY_MASK_CMD | KEY_V);
    }

    menu->add_separator();
    menu->add_item(RTR("Select All"), MENU_SELECT_ALL, KEY_MASK_CMD | KEY_A);

    if (!readonly) {
        menu->add_item(RTR("Clear"), MENU_CLEAR);
    }

    update();
}

bool TextEdit::is_readonly() const {

    return readonly;
}

void TextEdit::set_wrap_enabled(bool p_wrap_enabled) {

    D()->wrap_enabled = p_wrap_enabled;
}

bool TextEdit::is_wrap_enabled() const {

    return D()->wrap_enabled;
}


void TextEdit::_reset_caret_blink_timer() {
    if (caret_blink_enabled) {
        draw_caret = true;
        if (has_focus()) {
        caret_blink_timer->stop();
        caret_blink_timer->start();
        update();
        }
    }
}

void TextEdit::_toggle_draw_caret() {
    draw_caret = !draw_caret;
    if (is_visible_in_tree() && has_focus() && window_has_focus) {
        update();
    }
}

void TextEdit::_update_caches() {
    D()->_update_caches(this);
    D()->text.set_font(D()->cache.font);
}

SyntaxHighlighter *TextEdit::_get_syntax_highlighting() {
    return D()->syntax_highlighter;
}

void TextEdit::_set_syntax_highlighting(SyntaxHighlighter *p_syntax_highlighter) {
    D()->syntax_highlighter = p_syntax_highlighter;
    if (D()->syntax_highlighter) {
        D()->syntax_highlighter->set_text_editor(this);
        D()->syntax_highlighter->_update_cache();
    }
    D()->syntax_highlighting_cache.clear();
    update();
}

int TextEdit::_is_line_in_region(int p_line) {
    return D()->_is_line_in_region(p_line);
}

TextEdit::ColorRegionData TextEdit::_get_color_region(int p_region) const {
    TextEdit::ColorRegionData crd;
    auto v=D()->_get_color_region(p_region);
    crd.color=v.color;
    crd.begin_key_len=v.begin_key.length();
    crd.end_key_len=v.end_key.length();
    crd.line_only=v.line_only;
    crd.eq = v.eq;
    return crd;
}

Map<int, TextColorRegionInfo> TextEdit::_get_line_color_region_info(int p_line) const {
    return D()->_get_line_color_region_info(p_line);
}

void TextEdit::clear_colors() {

    D()->clear_colors();
    update();
}

void TextEdit::add_keyword_color(StringView p_keyword, const Color &p_color) {

    D()->keywords[StringUtils::from_utf8(p_keyword)] = p_color;
    D()->syntax_highlighting_cache.clear();
    update();
}

bool TextEdit::has_keyword_color_uistr(const UIString& p_keyword) const {
    return D()->keywords.contains(p_keyword);
}
bool TextEdit::has_keyword_color(StringView p_keyword) const {
    return D()->keywords.contains(StringUtils::from_utf8(p_keyword));
}
Color TextEdit::get_keyword_color_uistr(const UIString& p_keyword) const {

    auto iter = D()->keywords.find(p_keyword);
    ERR_FAIL_COND_V(iter==D()->keywords.end(), Color());
    return iter->second;
}
Color TextEdit::get_keyword_color(StringView p_keyword) const {

    auto iter = D()->keywords.find(StringUtils::from_utf8(p_keyword));
    ERR_FAIL_COND_V(iter==D()->keywords.end(), Color());
    return iter->second;
}

void TextEdit::add_color_region(StringView p_begin_key, StringView p_end_key, const Color &p_color, bool p_line_only) {

    D()->color_regions.emplace_back(
            StringUtils::from_utf8(p_begin_key), StringUtils::from_utf8(p_end_key), p_color, p_line_only);
    D()->syntax_highlighting_cache.clear();
    D()->text.clear_width_cache();
    update();
}

void TextEdit::add_member_keyword(StringView p_keyword, const Color &p_color) {
    D()->member_keywords[StringUtils::from_utf8(p_keyword)] = p_color;
    D()->syntax_highlighting_cache.clear();
    update();
}

bool TextEdit::has_member_color(const UIString& p_member) const {
    return D()->member_keywords.contains(p_member);
}

Color TextEdit::get_member_color(const UIString& p_member) const {
    return D()->member_keywords.at(p_member);
}

void TextEdit::clear_member_keywords() {
    D()->member_keywords.clear();
    D()->syntax_highlighting_cache.clear();
    update();
}

void TextEdit::set_syntax_coloring(bool p_enabled) {

    syntax_coloring = p_enabled;
    update();
}

bool TextEdit::is_syntax_coloring_enabled() const {

    return syntax_coloring;
}

void TextEdit::set_auto_indent(bool p_auto_indent) {
    auto_indent = p_auto_indent;
}

void TextEdit::cut() {

    if (!D()->selection.active) {

        UIString clipboard = D()->text[D()->cursor.line];
        OS::get_singleton()->set_clipboard(StringUtils::to_utf8(clipboard));
        cursor_set_line(D()->cursor.line);
        cursor_set_column(0);

        if (D()->cursor.line == 0 && get_line_count() > 1) {
            D()->_remove_text(D()->cursor.line, 0, D()->cursor.line + 1, 0);
        } else {
            D()->_remove_text(D()->cursor.line, 0, D()->cursor.line, D()->text[D()->cursor.line].length());
            backspace_at_cursor();
            cursor_set_line(D()->cursor.line + 1);
        }

        update();
        D()->cut_copy_line = clipboard;

    }  else {

        UIString clipboard = D()->_base_get_text(
                D()->selection.from_line, D()->selection.from_column, D()->selection.to_line, D()->selection.to_column);
        OS::get_singleton()->set_clipboard(StringUtils::to_utf8(clipboard).data());

        D()->_remove_text(
                D()->selection.from_line, D()->selection.from_column, D()->selection.to_line, D()->selection.to_column);
        cursor_set_line(D()->selection.from_line, false); // Set afterwards else it causes the view to be offset.
        cursor_set_column(D()->selection.from_column);

        D()->selection.active = false;
        D()->selection.selecting_mode = PrivateData::Selection::MODE_NONE;
        update();
        D()->cut_copy_line.clear();
    }
}

void TextEdit::copy() {

    if (!D()->selection.active) {

        if (D()->text[D()->cursor.line].length() != 0) {

            UIString clipboard =
                    D()->_base_get_text(D()->cursor.line, 0, D()->cursor.line, D()->text[D()->cursor.line].length());
            OS::get_singleton()->set_clipboard(StringUtils::to_utf8(clipboard).data());
            D()->cut_copy_line = clipboard;
        }
    } else {
        UIString clipboard = D()->_base_get_text(
                D()->selection.from_line, D()->selection.from_column, D()->selection.to_line, D()->selection.to_column);
        OS::get_singleton()->set_clipboard(StringUtils::to_utf8(clipboard).data());
        D()->cut_copy_line.clear();
    }
}

void TextEdit::paste() {

    D()->paste();
}

void TextEdit::select_all() {
    D()->select_all();
}

void TextEdit::deselect() {
    D()->deselect();
}

void TextEdit::select(int p_from_line, int p_from_column, int p_to_line, int p_to_column) {

    update();
}
void TextEdit::swap_lines(int line1, int line2) {
    String tmp = get_line(line1);
    String tmp2 = get_line(line2);
    set_line(line2, tmp);
    set_line(line1, tmp2);
}
bool TextEdit::is_selection_active() const {

    return D()->selection.active;
}
int TextEdit::get_selection_from_line() const {

    ERR_FAIL_COND_V(!D()->selection.active, -1);
    return D()->selection.from_line;
}
int TextEdit::get_selection_from_column() const {

    ERR_FAIL_COND_V(!D()->selection.active, -1);
    return D()->selection.from_column;
}
int TextEdit::get_selection_to_line() const {

    ERR_FAIL_COND_V(!D()->selection.active, -1);
    return D()->selection.to_line;
}
int TextEdit::get_selection_to_column() const {

    ERR_FAIL_COND_V(!D()->selection.active, -1);
    return D()->selection.to_column;
}

String TextEdit::get_selection_text() const {

    if (!D()->selection.active)
        return String();

    return StringUtils::to_utf8(D()->_base_get_text(
            D()->selection.from_line, D()->selection.from_column, D()->selection.to_line, D()->selection.to_column));
}

String TextEdit::get_word_under_cursor() const {

    int prev_cc = D()->cursor.column;
    while (prev_cc > 0) {
        bool is_char = _te_is_text_char(D()->text[D()->cursor.line][prev_cc - 1]);
        if (!is_char)
            break;
        --prev_cc;
    }

    int next_cc = D()->cursor.column;
    while (next_cc < D()->text[D()->cursor.line].length()) {
        bool is_char = _te_is_text_char(D()->text[D()->cursor.line][next_cc]);
        if (!is_char)
            break;
        ++next_cc;
    }
    if (prev_cc == D()->cursor.column || next_cc == D()->cursor.column)
        return String();
    return StringUtils::to_utf8(StringUtils::substr(D()->text[D()->cursor.line],prev_cc, next_cc - prev_cc));
}

void TextEdit::set_search_text(const UIString &p_search_text) {
    D()->search_text = p_search_text;
}

void TextEdit::set_search_flags(uint32_t p_flags) {
    D()->search_flags = p_flags;
}

void TextEdit::set_current_search_result(int line, int col) {
    search_result_line = line;
    search_result_col = col;
    update();
}

void TextEdit::set_highlight_all_occurrences(const bool p_enabled) {
    highlight_all_occurrences = p_enabled;
    update();
}

bool TextEdit::is_highlight_all_occurrences_enabled() const {
    return highlight_all_occurrences;
}

PoolVector<int> TextEdit::_search_bind(
        StringView _key, uint32_t p_search_flags, int p_from_line, int p_from_column) const {
    return D()->_search_bind(_key,p_search_flags,p_from_line,p_from_column);
}

bool TextEdit::search(const UIString &p_key, uint32_t p_search_flags, int p_from_line, int p_from_column, int &r_line,
        int &r_column) const {

    if (p_key.length() == 0)
        return false;
    ERR_FAIL_INDEX_V(p_from_line, D()->text.size(), false);
    ERR_FAIL_INDEX_V(p_from_column, D()->text[p_from_line].length() + 1, false);

    // Search through the whole document, but start by current line.

    int line = p_from_line;
    int pos = -1;

    for (uint32_t i = 0; i < D()->text.size() + 1; i++) {

        if (line < 0) {
            line = D()->text.size() - 1;
        }
        if (line == D()->text.size()) {
            line = 0;
        }

        UIString text_line = D()->text[line];
        int from_column = 0;
        if (line == p_from_line) {

            if (i == D()->text.size()) {
                // Wrapped.

                if (p_search_flags & SEARCH_BACKWARDS) {
                    from_column = text_line.length();
                } else {
                    from_column = 0;
                }

            } else {

                from_column = p_from_column;
            }

        } else {
            if (p_search_flags & SEARCH_BACKWARDS)
                from_column = text_line.length() - 1;
            else
                from_column = 0;
        }

        pos = -1;

        int pos_from = (p_search_flags & SEARCH_BACKWARDS) ? text_line.length() : 0;
        int last_pos = -1;

        while (true) {

            if (p_search_flags & SEARCH_BACKWARDS) {
                while ((last_pos = (p_search_flags & SEARCH_MATCH_CASE) ?
                                           StringUtils::rfind(text_line, p_key, pos_from) :
                                           StringUtils::rfindn(text_line, p_key, pos_from)) != -1) {
                    if (last_pos <= from_column) {
                        pos = last_pos;
                        break;
                    }
                    pos_from = last_pos - p_key.length();
                    if (pos_from < 0) {
                        break;
                    }
                }
            } else {
                while ((last_pos = (p_search_flags & SEARCH_MATCH_CASE) ?
                                           StringUtils::find(text_line, p_key, pos_from) :
                                           StringUtils::findn(text_line, p_key, pos_from)) != -1) {
                    if (last_pos >= from_column) {
                        pos = last_pos;
                        break;
                    }
                    pos_from = last_pos + p_key.length();
                }
            }

            bool is_match = true;

            if (pos != -1 && (p_search_flags & SEARCH_WHOLE_WORDS)) {
                // Validate for whole words.
                if (pos > 0 && _te_is_text_char(text_line[pos - 1]))
                    is_match = false;
                else if (pos + p_key.length() < text_line.length() && _te_is_text_char(text_line[pos + p_key.length()]))
                    is_match = false;
            }

            if (pos_from == -1) {
                pos = -1;
            }

            if (is_match || last_pos == -1 || pos == -1) {
                break;
            }

            pos_from = (p_search_flags & SEARCH_BACKWARDS) ? pos - 1 : pos + 1;
            pos = -1;
        }

        if (pos != -1)
            break;

        if (p_search_flags & SEARCH_BACKWARDS)
            line--;
        else
            line++;
    }

    if (pos == -1) {
        r_line = -1;
        r_column = -1;
        return false;
    }

    r_line = line;
    r_column = pos;

    return true;
}

void TextEdit::_cursor_changed_emit() {

    emit_signal("cursor_changed");
    D()->cursor_changed_dirty = false;
}

void TextEdit::_text_changed_emit() {

    emit_signal("text_changed");
    D()->text_changed_dirty = false;
}

void TextEdit::set_line_as_marked(int p_line, bool p_marked) {

    ERR_FAIL_INDEX(p_line, D()->text.size());
    D()->text.set_marked(p_line, p_marked);
    update();
}

void TextEdit::set_line_as_safe(int p_line, bool p_safe) {
    ERR_FAIL_INDEX(p_line, D()->text.size());
    D()->text.set_safe(p_line, p_safe);
    update();
}

bool TextEdit::is_line_set_as_safe(int p_line) const {
    ERR_FAIL_INDEX_V(p_line, D()->text.size(), false);
    return D()->text.is_safe(p_line);
}

void TextEdit::set_executing_line(int p_line) {
    ERR_FAIL_INDEX(p_line, D()->text.size());
    executing_line = p_line;
    update();
}

void TextEdit::clear_executing_line() {
    executing_line = -1;
    update();
}

bool TextEdit::is_line_set_as_bookmark(int p_line) const {

    ERR_FAIL_INDEX_V(p_line, D()->text.size(), false);
    return D()->text.is_bookmark(p_line);
}

void TextEdit::set_line_as_bookmark(int p_line, bool p_bookmark) {

    ERR_FAIL_INDEX(p_line, D()->text.size());
    D()->text.set_bookmark(p_line, p_bookmark);
    update();
}

void TextEdit::get_bookmarks(Vector<int> *p_bookmarks) const {

    for (uint32_t i = 0; i < D()->text.size(); i++) {
        if (D()->text.is_bookmark(i))
            p_bookmarks->push_back(i);
    }
}

Array TextEdit::get_bookmarks_array() const {

    Array arr;
    for (uint32_t i = 0; i < D()->text.size(); i++) {
        if (D()->text.is_bookmark(i))
            arr.append(i);
    }
    return arr;
}

bool TextEdit::is_line_set_as_breakpoint(int p_line) const {

    ERR_FAIL_INDEX_V(p_line, D()->text.size(), false);
    return D()->text.is_breakpoint(p_line);
}

void TextEdit::set_line_as_breakpoint(int p_line, bool p_breakpoint) {

    ERR_FAIL_INDEX(p_line, D()->text.size());
    D()->text.set_breakpoint(p_line, p_breakpoint);
    update();
}

void TextEdit::get_breakpoints(Vector<int> *p_breakpoints) const {

    for (uint32_t i = 0; i < D()->text.size(); i++) {
        if (D()->text.is_breakpoint(i))
            p_breakpoints->push_back(i);
    }
}

Array TextEdit::get_breakpoints_array() const {

    Array arr;
    for (int i = 0; i < D()->text.size(); i++) {
        if (D()->text.is_breakpoint(i))
            arr.append(i);
    }
    return arr;
}

void TextEdit::remove_breakpoints() {
    for (size_t i = 0; i < D()->text.size(); i++) {
        if (D()->text.is_breakpoint(i))
            /* Should "breakpoint_toggled" be fired when breakpoints are removed this way? */
            D()->text.set_breakpoint(i, false);
    }
}

void TextEdit::set_line_info_icon(int p_line, const Ref<Texture>& p_icon, StringName p_info) {
    ERR_FAIL_INDEX(p_line, D()->text.size());
    D()->text.set_info_icon(p_line, p_icon, eastl::move(p_info));
    update();
}

void TextEdit::clear_info_icons() {
    D()->text.clear_info_icons();
    update();
}

void TextEdit::set_line_as_hidden(int p_line, bool p_hidden) {

    ERR_FAIL_INDEX(p_line, D()->text.size());
    if (is_hiding_enabled() || !p_hidden)
        D()->text.set_hidden(p_line, p_hidden);
    update();
}

bool TextEdit::is_line_hidden(int p_line) const {
    return D()->is_line_hidden(p_line);
}

void TextEdit::fold_all_lines() {

    for (uint32_t i = 0; i < D()->text.size(); i++) {
        fold_line(i);
    }
    _update_scrollbars();
    update();
}

void TextEdit::unhide_all_lines() {

    for (uint32_t i = 0; i < D()->text.size(); i++) {
        D()->text.set_hidden(i, false);
    }
    _update_scrollbars();
    update();
}

int TextEdit::num_lines_from(int p_line_from, int visible_amount) const {
    return D()->num_lines_from(p_line_from,visible_amount);
}

int TextEdit::num_lines_from_rows(int p_line_from, int p_wrap_index_from, int visible_amount, int &wrap_index) const {

    // Returns the number of lines (hidden and unhidden) from (p_line_from + p_wrap_index_from) row to (p_line_from +
    // visible_amount of unhidden and wrapped rows). Wrap index is set to the wrap index of the last line.
    wrap_index = 0;
    ERR_FAIL_INDEX_V(p_line_from, D()->text.size(), ABS(visible_amount));

    if (!is_hiding_enabled() && !is_wrap_enabled())
        return ABS(visible_amount);

    int num_visible = 0;
    int num_total = 0;
    if (visible_amount == 0) {
        num_total = 0;
        wrap_index = 0;
    } else if (visible_amount > 0) {
        int i;
        num_visible -= p_wrap_index_from;
        for (i = p_line_from; i < D()->text.size(); i++) {
            num_total++;
            if (!is_line_hidden(i)) {
                num_visible++;
                num_visible += get_line_wrap_count(i);
            }
            if (num_visible >= visible_amount)
                break;
        }
        wrap_index = get_line_wrap_count(MIN(i, D()->text.size() - 1)) - (num_visible - visible_amount);
    } else {
        visible_amount = ABS(visible_amount);
        int i;
        num_visible -= get_line_wrap_count(p_line_from) - p_wrap_index_from;
        for (i = p_line_from; i >= 0; i--) {
            num_total++;
            if (!is_line_hidden(i)) {
                num_visible++;
                num_visible += get_line_wrap_count(i);
            }
            if (num_visible >= visible_amount)
                break;
        }
        wrap_index = (num_visible - visible_amount);
    }
    wrap_index = eastl::max(wrap_index, 0);
    return num_total;
}

int TextEdit::get_last_unhidden_line() const {

    // Returns the last line in the text that is not hidden.
    if (!is_hiding_enabled())
        return D()->text.size() - 1;

    int last_line;
    for (last_line = D()->text.size() - 1; last_line > 0; last_line--) {
        if (!is_line_hidden(last_line)) {
            break;
        }
    }
    return last_line;
}

int TextEdit::get_indent_level(int p_line) const {

    ERR_FAIL_INDEX_V(p_line, D()->text.size(), 0);

    // Counts number of tabs and spaces before line starts.
    int tab_count = 0;
    int whitespace_count = 0;
    int line_length = D()->text[p_line].size();
    for (int i = 0; i < line_length - 1; i++) {
        if (D()->text[p_line][i] == '\t') {
            tab_count++;
        } else if (D()->text[p_line][i] == ' ') {
            whitespace_count++;
        } else {
            break;
        }
    }
    return tab_count * indent_size + whitespace_count;
}

bool TextEdit::is_line_comment(int p_line) const {

    // Checks to see if this line is the start of a comment.
    ERR_FAIL_INDEX_V(p_line, D()->text.size(), false);

    const Map<int, TextColorRegionInfo> &cri_map = D()->text.get_color_region_info(p_line);

    int line_length = D()->text[p_line].size();
    for (int i = 0; i < line_length - 1; i++) {
        if (_is_symbol(D()->text[p_line][i]) && cri_map.contains(i)) {
            const TextColorRegionInfo &cri = cri_map.at(i);
            return D()->color_regions[cri.region].begin_key == "#" || D()->color_regions[cri.region].begin_key == "//";
        } else if (_is_whitespace(D()->text[p_line][i])) {
            continue;
        } else {
            break;
        }
    }
    return false;
}

bool TextEdit::can_fold(int p_line) const {

    ERR_FAIL_INDEX_V(p_line, D()->text.size(), false);
    if (!is_hiding_enabled())
        return false;
    if (p_line + 1 >= D()->text.size())
        return false;
    if (StringUtils::strip_edges(D()->text[p_line]).isEmpty())
        return false;
    if (is_folded(p_line))
        return false;
    if (is_line_hidden(p_line))
        return false;
    if (is_line_comment(p_line))
        return false;

    int start_indent = get_indent_level(p_line);

    for (int i = p_line + 1; i < D()->text.size(); i++) {
        if (StringUtils::strip_edges(D()->text[i]).isEmpty())
            continue;
        int next_indent = get_indent_level(i);
        if (is_line_comment(i)) {
            continue;
        } else if (next_indent > start_indent) {
            return true;
        } else {
            return false;
        }
    }

    return false;
}

bool TextEdit::is_folded(int p_line) const {

    ERR_FAIL_INDEX_V(p_line, D()->text.size(), false);
    if (p_line + 1 >= D()->text.size())
        return false;
    return !is_line_hidden(p_line) && is_line_hidden(p_line + 1);
}

Vector<int> TextEdit::get_folded_lines() const {
    Vector<int> folded_lines;

    for (int i = 0; i < D()->text.size(); i++) {
        if (is_folded(i)) {
            folded_lines.push_back(i);
        }
    }
    return folded_lines;
}

void TextEdit::fold_line(int p_line) {

    ERR_FAIL_INDEX(p_line, D()->text.size());
    if (!is_hiding_enabled())
        return;
    if (!can_fold(p_line))
        return;

    // Hide lines below this one.
    int start_indent = get_indent_level(p_line);
    int last_line = start_indent;
    for (int i = p_line + 1; i < D()->text.size(); i++) {
        if (!StringUtils::strip_edges(D()->text[i]).isEmpty()) {
            if (is_line_comment(i)) {
                continue;
            } else if (get_indent_level(i) > start_indent) {
                last_line = i;
            } else {
                break;
            }
        }
    }
    for (int i = p_line + 1; i <= last_line; i++) {
        set_line_as_hidden(i, true);
    }

    // Fix selection.
    if (is_selection_active()) {
        if (is_line_hidden(D()->selection.from_line) && is_line_hidden(D()->selection.to_line)) {
            deselect();
        } else if (is_line_hidden(D()->selection.from_line)) {
            select(p_line, 9999, D()->selection.to_line, D()->selection.to_column);
        } else if (is_line_hidden(D()->selection.to_line)) {
            select(D()->selection.from_line, D()->selection.from_column, p_line, 9999);
        }
    }

    // Reset D()->cursor.
    if (is_line_hidden(D()->cursor.line)) {
        cursor_set_line(p_line, false, false);
        cursor_set_column(get_line(p_line).length(), false);
    }
    _update_scrollbars();
    update();
}

void TextEdit::unfold_line(int p_line) {

    ERR_FAIL_INDEX(p_line, D()->text.size());

    if (!is_folded(p_line) && !is_line_hidden(p_line))
        return;
    int fold_start;
    for (fold_start = p_line; fold_start > 0; fold_start--) {
        if (is_folded(fold_start))
            break;
    }
    fold_start = is_folded(fold_start) ? fold_start : p_line;

    for (int i = fold_start + 1; i < D()->text.size(); i++) {
        if (is_line_hidden(i)) {
            set_line_as_hidden(i, false);
        } else {
            break;
        }
    }
    _update_scrollbars();
    update();
}

void TextEdit::toggle_fold_line(int p_line) {

    ERR_FAIL_INDEX(p_line, D()->text.size());

    if (!is_folded(p_line))
        fold_line(p_line);
    else
        unfold_line(p_line);
}

String TextEdit::get_text_utf8() const {
    String longthing;
    int len = D()->text.size();
    for (int i = 0; i < len; i++) {

        longthing += StringUtils::to_utf8(D()->text[i]).data();
        if (i != len - 1)
            longthing += "\n";
    }

    return longthing;
}

int TextEdit::get_line_count() const {

    return D()->text.size();
}

bool TextEdit::has_undo() const {
    return D()->has_undo();
}

bool TextEdit::has_redo() const {
    return D()->has_redo();
}
void TextEdit::undo() {

    D()->undo();

}

void TextEdit::redo() {
    D()->redo();
}

void TextEdit::clear_undo_history() {
    D()->clear_undo_history();
}

void TextEdit::begin_complex_operation() {
    D()->begin_complex_operation();
}

void TextEdit::end_complex_operation() {

    D()->end_complex_operation();
}

void TextEdit::set_indent_using_spaces(const bool p_use_spaces) {
    indent_using_spaces = p_use_spaces;
}

bool TextEdit::is_indent_using_spaces() const {
    return indent_using_spaces;
}

void TextEdit::set_indent_size(const int p_size) {
    ERR_FAIL_COND(p_size <= 0);
    indent_size = p_size;
    D()->text.set_indent_size(p_size);

    D()->space_indent.resize(p_size,' ');

    update();
}

int TextEdit::get_indent_size() {

    return indent_size;
}

void TextEdit::set_draw_tabs(bool p_draw) {

    draw_tabs = p_draw;
    update();
}

bool TextEdit::is_drawing_tabs() const {

    return draw_tabs;
}

void TextEdit::set_draw_spaces(bool p_draw) {

    draw_spaces = p_draw;
}

bool TextEdit::is_drawing_spaces() const {

    return draw_spaces;
}

void TextEdit::set_override_selected_font_color(bool p_override_selected_font_color) {
    override_selected_font_color = p_override_selected_font_color;
}

bool TextEdit::is_overriding_selected_font_color() const {
    return override_selected_font_color;
}

void TextEdit::set_insert_mode(bool p_enabled) {
    insert_mode = p_enabled;
    update();
}

bool TextEdit::is_insert_mode() const {
    return insert_mode;
}

bool TextEdit::is_insert_text_operation() {
    return (D()->current_op.type == PrivateData::TextOperation::TYPE_INSERT);
}

void TextEdit::set_text(StringView p_text) {
    set_text_ui(StringUtils::from_utf8(p_text));
}

uint32_t TextEdit::get_version() const {
    return D()->current_op.version;
}

uint32_t TextEdit::get_saved_version() const {

    return D()->saved_version;
}

void TextEdit::tag_saved_version() {

    D()->saved_version = get_version();
}

double TextEdit::get_scroll_pos_for_line(int p_line, int p_wrap_index) const {

    if (!is_wrap_enabled() && !is_hiding_enabled())
        return p_line;

    // Count the number of visible lines up to this line.
    double new_line_scroll_pos = 0;
    int to = CLAMP<int>(p_line, 0, D()->text.size() - 1);
    for (int i = 0; i < to; i++) {
        if (!D()->text.is_hidden(i)) {
            new_line_scroll_pos++;
            new_line_scroll_pos += get_line_wrap_count(i);
        }
    }
    new_line_scroll_pos += p_wrap_index;
    return new_line_scroll_pos;
}

void TextEdit::set_line_as_first_visible(int p_line, int p_wrap_index) {

    set_v_scroll(get_scroll_pos_for_line(p_line, p_wrap_index));
}

void TextEdit::set_line_as_center_visible(int p_line, int p_wrap_index) {

    int visible_rows = get_visible_rows();
    int wi;
    int first_line = p_line - num_lines_from_rows(p_line, p_wrap_index, -visible_rows / 2, wi) + 1;

    set_v_scroll(get_scroll_pos_for_line(first_line, wi));
}

void TextEdit::set_line_as_last_visible(int p_line, int p_wrap_index) {

    int wi;
    int first_line = p_line - num_lines_from_rows(p_line, p_wrap_index, -get_visible_rows() - 1, wi) + 1;

    set_v_scroll(get_scroll_pos_for_line(first_line, wi) + get_visible_rows_offset());
}

int TextEdit::get_first_visible_line() const {

    return CLAMP<int>(D()->cursor.line_ofs, 0, D()->text.size() - 1);
}

int TextEdit::get_last_full_visible_line() const {

    int first_vis_line = get_first_visible_line();
    int last_vis_line = 0;
    int wi;
    last_vis_line =
            first_vis_line + num_lines_from_rows(first_vis_line, D()->cursor.wrap_ofs, get_visible_rows(), wi) - 1;
    last_vis_line = CLAMP<int>(last_vis_line, 0, D()->text.size() - 1);
    return last_vis_line;
}

int TextEdit::get_last_full_visible_line_wrap_index() const {

    int first_vis_line = get_first_visible_line();
    int wi;
    num_lines_from_rows(first_vis_line, D()->cursor.wrap_ofs, get_visible_rows(), wi);
    return wi;
}

double TextEdit::get_visible_rows_offset() const {

    float total = _get_control_height();
    total /= (double)get_row_height();
    total = total - floor(total);
    total = -CLAMP(total, 0.001f, 1.0f) + 1;
    return total;
}

double TextEdit::get_v_scroll_offset() const {

    float val = get_v_scroll() - floor(get_v_scroll());
    return CLAMP(val, 0.f, 1.0f);
}

double TextEdit::get_v_scroll() const {

    return v_scroll->get_value();
}

void TextEdit::set_v_scroll(double p_scroll) {

    v_scroll->set_value(p_scroll);
    int max_v_scroll = v_scroll->get_max() - v_scroll->get_page();
    if (p_scroll >= max_v_scroll - 1.0f)
        _scroll_moved(v_scroll->get_value());
}

int TextEdit::get_h_scroll() const {

    return h_scroll->get_value();
}

void TextEdit::set_h_scroll(int p_scroll) {

    if (p_scroll < 0) {
        p_scroll = 0;
    }
    h_scroll->set_value(p_scroll);
}

void TextEdit::set_smooth_scroll_enabled(bool p_enable) {

    v_scroll->set_smooth_scroll_enabled(p_enable);
    smooth_scroll_enabled = p_enable;
}

bool TextEdit::is_smooth_scroll_enabled() const {

    return smooth_scroll_enabled;
}

void TextEdit::set_v_scroll_speed(float p_speed) {

    v_scroll_speed = p_speed;
}

float TextEdit::get_v_scroll_speed() const {

    return v_scroll_speed;
}

void TextEdit::set_completion(bool p_enabled, const Vector<UIString> &p_prefixes) {

    D()->completion_prefixes.clear();
    D()->completion_enabled = p_enabled;
    for (int i = 0; i < p_prefixes.size(); i++)
        D()->completion_prefixes.insert(p_prefixes[i]);
}

void TextEdit::_confirm_completion() {

    begin_complex_operation();

    D()->_remove_text(
            D()->cursor.line, D()->cursor.column - D()->completion_base.length(), D()->cursor.line, D()->cursor.column);
    cursor_set_column(D()->cursor.column - D()->completion_base.length(), false);
    insert_text_at_cursor_ui(StringUtils::from_utf8(D()->completion_current.insert_text));

    // When inserted into the middle of an existing string/method, don't add an unnecessary quote/bracket.
    UIString line = D()->text[D()->cursor.line];
    CharType next_char = line[D()->cursor.column];
    CharType last_completion_char =
            D()->completion_current.insert_text[D()->completion_current.insert_text.length() - 1];
    CharType last_completion_char_display =
            D()->completion_current.display[D()->completion_current.display.length() - 1];


    if ((last_completion_char == '"' || last_completion_char == '\'') &&
            (last_completion_char == next_char || last_completion_char_display == next_char)) {
        D()->_remove_text(D()->cursor.line, D()->cursor.column, D()->cursor.line, D()->cursor.column + 1);
    }

    if (last_completion_char == '(') {

        if (next_char == last_completion_char) {
            D()->_remove_text(D()->cursor.line, D()->cursor.column - 1, D()->cursor.line, D()->cursor.column);
        } else if (auto_brace_completion_enabled) {
            insert_text_at_cursor_ui(UIString(")"));
            D()->cursor.column--;
        }
    } else if (last_completion_char == ')' && next_char == '(') {

        D()->_remove_text(D()->cursor.line, D()->cursor.column - 2, D()->cursor.line, D()->cursor.column);
        if (line[D()->cursor.column + 1] != ')') {
            D()->cursor.column--;
        }
    }

    D()->end_complex_operation();

    D()->_cancel_completion();

    if (last_completion_char == '(') {
        query_code_comple();
    }
}

static bool _is_completable(CharType c) {

    return !_is_symbol(c) || c == '"' || c == '\'';
}

void TextEdit::_update_completion_candidates() {

    String l = StringUtils::to_utf8(D()->text[D()->cursor.line]).data();
    int cofs = CLAMP<int>(D()->cursor.column, 0, l.length());

    String s;

    // Look for keywords first.

    bool inquote = false;
    int first_quote = -1;
    int restore_quotes = -1;

    int c = cofs - 1;
    while (c >= 0) {
        if (l[c] == '"' || l[c] == '\'') {
            inquote = !inquote;
            if (first_quote == -1)
                first_quote = c;
            restore_quotes = 0;
        } else if (restore_quotes == 0 && l[c] == '$') {
            restore_quotes = 1;
        } else if (restore_quotes == 0 && !_is_whitespace(l[c])) {
            restore_quotes = -1;
        }
        c--;
    }

    bool pre_keyword = false;
    bool cancel = false;

    if (!inquote && first_quote == cofs - 1) {
        // No completion here.
        cancel = true;
    } else if (inquote && first_quote != -1) {

        s = StringUtils::substr(l,first_quote, cofs - first_quote);
    } else if (cofs > 0 && l[cofs - 1] == ' ') {
        int kofs = cofs - 1;
        UIString kw;
        while (kofs >= 0 && l[kofs] == ' ')
            kofs--;

        while (kofs >= 0 && l[kofs] > 32 && _is_completable(l[kofs])) {
            kw = l[kofs] + kw;
            kofs--;
        }

        pre_keyword = D()->keywords.contains(kw);

    } else {

        while (cofs > 0 && l[cofs - 1] > 32 && (l[cofs - 1] == '/' || _is_completable(l[cofs - 1]))) {
            s = l[cofs - 1] + s;
            if (l[cofs - 1] == '\'' || l[cofs - 1] == '"' || l[cofs - 1] == '$')
                break;

            cofs--;
        }
    }

    if (D()->cursor.column > 0 && l[D()->cursor.column - 1] == '(' && !pre_keyword && !D()->completion_forced) {
        cancel = true;
    }

    update();

    bool prev_is_prefix = false;
    if (cofs > 0 && D()->completion_prefixes.contains(UIString(l[cofs - 1])))
        prev_is_prefix = true;
    // Check with one space before prefix, to allow indent.
    if (cofs > 1 && l[cofs - 1] == ' ' && D()->completion_prefixes.contains(UIString(l[cofs - 2])))
        prev_is_prefix = true;

    if (cancel || (!pre_keyword && s.empty() && (cofs == 0 || !prev_is_prefix))) {
        // None to complete, cancel.
        D()->_cancel_completion();
        return;
    }

    D()->completion_options.clear();
    D()->completion_index = 0;
    D()->completion_base = s;
    Vector<float> sim_cache;
    bool single_quote = StringUtils::begins_with(s,"'");
    Vector<ScriptCodeCompletionOption> completion_options_casei;

    for (ScriptCodeCompletionOption& option : D()->completion_sources) {

        if (single_quote && StringUtils::is_quoted(option.display)) {
            option.display = StringUtils::quote(StringUtils::unquote(option.display),'\'');
        }

        if (inquote && restore_quotes == 1 && !StringUtils::is_quoted(option.display)) {
            char quote = single_quote ? '\'' : '\"';
            option.display = StringUtils::quote(option.display,quote);
            option.insert_text = StringUtils::quote(option.insert_text,quote);
        }

        if (StringUtils::begins_with(option.display,s)) {
            D()->completion_options.push_back(option);
        } else if (StringUtils::begins_with(StringUtils::to_lower(option.display),StringUtils::to_lower(s))) {
            completion_options_casei.push_back(option);
        }
    }

    D()->completion_options.push_back(completion_options_casei);

    if (D()->completion_options.empty()) {
        for (int i = 0; i < D()->completion_sources.size(); i++) {
            if (StringUtils::is_subsequence_of(s,D()->completion_sources[i].display)) {
                D()->completion_options.push_back(D()->completion_sources[i]);
            }
        }
    }

    if (D()->completion_options.empty()) {
        for (int i = 0; i < D()->completion_sources.size(); i++) {
            if (StringUtils::is_subsequence_of(s,D()->completion_sources[i].display,StringUtils::CaseInsensitive)) {
                D()->completion_options.push_back(D()->completion_sources[i]);
            }
        }
    }

    if (D()->completion_options.empty()) {
        // No options to complete, cancel.
        D()->_cancel_completion();
        return;
    }

    if (D()->completion_options.size() == 1 && s == D()->completion_options[0].display) {
        // A perfect match, stop completion.
        D()->_cancel_completion();
        return;
    }

    // The top of the list is the best match.
    D()->completion_current = D()->completion_options.front();
    D()->completion_enabled = true;
}

void TextEdit::query_code_comple() {

    UIString l = D()->text[D()->cursor.line];
    int ofs = CLAMP(D()->cursor.column, 0, l.length());

    bool inquote = false;

    int c = ofs - 1;
    while (c >= 0) {
        if (l[c] == '"' || l[c] == '\'')
            inquote = !inquote;
        c--;
    }

    bool ignored = D()->completion_active && !D()->completion_options.empty();
    if (ignored) {
        ScriptCodeCompletionOption::Kind kind = ScriptCodeCompletionOption::KIND_PLAIN_TEXT;
        const ScriptCodeCompletionOption *previous_option = nullptr;
        for (int i = 0; i < D()->completion_options.size(); i++) {
            const ScriptCodeCompletionOption &current_option = D()->completion_options[i];
            if (!previous_option) {
                previous_option = &current_option;
                kind = current_option.kind;
            }
            if (previous_option->kind != current_option.kind) {
                ignored = false;
                break;
            }
        }
        ignored = ignored && (kind == ScriptCodeCompletionOption::KIND_FILE_PATH ||
                                     kind == ScriptCodeCompletionOption::KIND_NODE_PATH ||
                                     kind == ScriptCodeCompletionOption::KIND_SIGNAL);
    }

    if (!ignored) {
        if (ofs > 0 &&
                (inquote || _is_completable(l[ofs - 1]) || D()->completion_prefixes.contains(UIString(l[ofs - 1]))))
            emit_signal("request_completion");
        else if (ofs > 1 && l[ofs - 1] == ' ' &&
                 D()->completion_prefixes.contains(
                         UIString(l[ofs - 2]))) // Make it work with a space too, it's good enough.
            emit_signal("request_completion");
    }
}

void TextEdit::set_code_hint(const String &p_hint) {

    D()->completion_hint = p_hint;
    D()->completion_hint_offset = -0xFFFF;
    update();
}

void TextEdit::code_complete(const Vector<ScriptCodeCompletionOption> &p_strings, bool p_forced) {

    D()->completion_sources = p_strings;
    D()->completion_active = true;
    D()->completion_forced = p_forced;
    D()->completion_current = ScriptCodeCompletionOption();
    D()->completion_index = 0;
    _update_completion_candidates();
}

String TextEdit::get_word_at_pos(const Vector2 &p_pos) const {

    int row, col;
    _get_mouse_pos(p_pos, row, col);

    UIString s = D()->text[row];
    if (s.length() == 0)
        return String();
    int beg, end;
    if (select_word(s, col, beg, end)) {

        bool inside_quotes = false;
        CharType selected_quote = '\0';
        int qbegin = 0, qend = 0;
        for (int i = 0; i < s.length(); i++) {
            if (s[i] == '"' || s[i] == '\'') {
                if (i == 0 || s[i - 1] != '\\') {
                    if (inside_quotes && selected_quote == s[i]) {
                        qend = i;
                        inside_quotes = false;
                        selected_quote = '\0';
                        if (col >= qbegin && col <= qend) {
                            return String(s.midRef(qbegin, qend - qbegin).toUtf8().data());
                        }
                    } else if (!inside_quotes) {
                        qbegin = i + 1;
                        inside_quotes = true;
                        selected_quote = s[i];
                    }
                }
            }
        }

        return String(s.midRef(beg, end - beg).toUtf8().data());
    }

    return String();
}

const String & TextEdit::get_tooltip(const Point2 &p_pos) const {

    Object *tooltip_obj = object_for_entity(tooltip_obj_id);
    if (!tooltip_obj)
        return Control::get_tooltip(p_pos);
    int row, col;
    _get_mouse_pos(p_pos, row, col);

    UIString s = D()->text[row];
    if (s.length() == 0)
        return Control::get_tooltip(p_pos);
    int beg, end;
    if (select_word(s, col, beg, end)) {
        //TODO: this will not work well in multi-threaded context
        static String selected_tooltip;
        selected_tooltip = tooltip_obj
                ->call_va(tooltip_func, StringUtils::to_utf8(StringUtils::substr(s, beg, end - beg)), tooltip_ud)
                .as<String>();
        return selected_tooltip;
    }

    return Control::get_tooltip(p_pos);
}

void TextEdit::set_tooltip_request_func(Object *p_obj, const StringName &p_function, const Variant &p_udata) {

    ERR_FAIL_NULL(p_obj);
    tooltip_obj_id = p_obj->get_instance_id();
    tooltip_func = p_function;
    tooltip_ud = p_udata;
}

void TextEdit::set_line(int line, StringView _new_text) {

    D()->set_line(line,_new_text);
}

void TextEdit::insert_at(const UIString &p_text, int at) {
    D()->insert_at(p_text,at);
}

void TextEdit::set_show_line_numbers(bool p_show) {

    line_numbers = p_show;
    update();
}

void TextEdit::set_line_numbers_zero_padded(bool p_zero_padded) {

    line_numbers_zero_padded = p_zero_padded;
    update();
}

bool TextEdit::is_show_line_numbers_enabled() const {
    return line_numbers;
}

void TextEdit::set_show_line_length_guidelines(bool p_show) {
    line_length_guidelines = p_show;
    update();
}

void TextEdit::set_line_length_guideline_soft_column(int p_column) {
    line_length_guideline_soft_col = p_column;
    update();
}

void TextEdit::set_line_length_guideline_hard_column(int p_column) {
    line_length_guideline_hard_col = p_column;
    update();
}

void TextEdit::set_bookmark_gutter_enabled(bool p_draw) {
    draw_bookmark_gutter = p_draw;
    update();
}

bool TextEdit::is_bookmark_gutter_enabled() const {
    return draw_bookmark_gutter;
}

void TextEdit::set_breakpoint_gutter_enabled(bool p_draw) {
    draw_breakpoint_gutter = p_draw;
    update();
}

bool TextEdit::is_breakpoint_gutter_enabled() const {
    return draw_breakpoint_gutter;
}

void TextEdit::set_breakpoint_gutter_width(int p_gutter_width) {
    breakpoint_gutter_width = p_gutter_width;
    update();
}

int TextEdit::get_breakpoint_gutter_width() const {
    return D()->cache.breakpoint_gutter_width;
}

void TextEdit::set_draw_fold_gutter(bool p_draw) {
    draw_fold_gutter = p_draw;
    update();
}

bool TextEdit::is_drawing_fold_gutter() const {
    return draw_fold_gutter;
}

void TextEdit::set_fold_gutter_width(int p_gutter_width) {
    fold_gutter_width = p_gutter_width;
    update();
}

int TextEdit::get_fold_gutter_width() const {
    return D()->cache.fold_gutter_width;
}

void TextEdit::set_draw_info_gutter(bool p_draw) {
    draw_info_gutter = p_draw;
    update();
}

bool TextEdit::is_drawing_info_gutter() const {
    return draw_info_gutter;
}

void TextEdit::set_info_gutter_width(int p_gutter_width) {
    info_gutter_width = p_gutter_width;
    update();
}

int TextEdit::get_info_gutter_width() const {
    return info_gutter_width;
}

int TextEdit::get_total_gutter_width() const {
    return D()->cache.line_number_w + D()->cache.breakpoint_gutter_width + D()->cache.fold_gutter_width + D()->cache.info_gutter_width;
}
void TextEdit::set_draw_minimap(bool p_draw) {
    draw_minimap = p_draw;
    update();
}

bool TextEdit::is_drawing_minimap() const {
    return draw_minimap;
}

void TextEdit::set_minimap_width(int p_minimap_width) {
    minimap_width = p_minimap_width;
    update();
}

int TextEdit::get_minimap_width() const {
    return minimap_width;
}

void TextEdit::set_hiding_enabled(bool p_enabled) {
    if (!p_enabled)
        unhide_all_lines();
    D()->hiding_enabled = p_enabled;
    update();
}

bool TextEdit::is_hiding_enabled() const {
    return D()->hiding_enabled;
}

void TextEdit::set_highlight_current_line(bool p_enabled) {
    highlight_current_line = p_enabled;
    update();
}

bool TextEdit::is_highlight_current_line_enabled() const {
    return highlight_current_line;
}

bool TextEdit::is_text_field() const {
    return true;
}

void TextEdit::menu_option(int p_option) {

    switch (p_option) {
        case MENU_CUT: {
            if (!readonly) {
                cut();
            }
        } break;
        case MENU_COPY: {
            copy();
        } break;
        case MENU_PASTE: {
            if (!readonly) {
                paste();
            }
        } break;
        case MENU_CLEAR: {
            if (!readonly) {
                clear();
            }
        } break;
        case MENU_SELECT_ALL: {
            select_all();
        } break;
        case MENU_UNDO: {
            undo();
        } break;
        case MENU_REDO: {
            redo();
        }
    }
}

void TextEdit::set_select_identifiers_on_hover(bool p_enable) {

    select_identifiers_enabled = p_enable;
}

bool TextEdit::is_selecting_identifiers_on_hover_enabled() const {

    return select_identifiers_enabled;
}

void TextEdit::set_context_menu_enabled(bool p_enable) {
    context_menu_enabled = p_enable;
}

bool TextEdit::is_context_menu_enabled() {
    return context_menu_enabled;
}

void TextEdit::set_shortcut_keys_enabled(bool p_enabled) {
    shortcut_keys_enabled = p_enabled;

    _generate_context_menu();
}

void TextEdit::set_middle_mouse_paste_enabled(bool p_enabled) {
    middle_mouse_paste_enabled = p_enabled;
}
void TextEdit::set_selecting_enabled(bool p_enabled) {
    D()->selecting_enabled = p_enabled;

    if (!D()->selecting_enabled)
        D()->deselect();

    _generate_context_menu();
}

bool TextEdit::is_selecting_enabled() const {
    return D()->selecting_enabled;
}

void TextEdit::set_deselect_on_focus_loss_enabled(const bool p_enabled) {
    D()->deselect_on_focus_loss_enabled = p_enabled;
    if (p_enabled && D()->selection.active && !has_focus()) {
        deselect();
    }
}

bool TextEdit::is_deselect_on_focus_loss_enabled() const {
    return D()->deselect_on_focus_loss_enabled;
}

bool TextEdit::is_shortcut_keys_enabled() const {
    return shortcut_keys_enabled;
}
bool TextEdit::is_middle_mouse_paste_enabled() const {
    return middle_mouse_paste_enabled;
}
PopupMenu *TextEdit::get_menu() const {
    return menu;
}
void TextEdit::_push_current_op() {
    D()->_push_current_op();
}
int TextEdit::get_line_width(int p_line, int p_wrap_index) const {
    ERR_FAIL_INDEX_V(p_line, D()->text.size(), 0);

    if (p_wrap_index >= 0 && is_line_wrapped(p_line)) {
        Vector<UIString> rows = get_wrap_rows_text(p_line);
        ERR_FAIL_INDEX_V(p_wrap_index, rows.size(), 0);

        int w = 0;
        int len = rows[p_wrap_index].length();
        const UIString &str = rows[p_wrap_index];
        for (int i = 0; i < len; i++) {
            w += D()->text.get_char_width(str[i], str[i + 1], w);
        }

        return w;
    }

    return D()->text.get_line_width(p_line);
}

int TextEdit::get_line_height() const {
    return get_row_height();
}
void TextEdit::_bind_methods() {

    SE_BIND_METHOD(TextEdit,_gui_input);
    SE_BIND_METHOD(TextEdit,_cursor_changed_emit);
    SE_BIND_METHOD(TextEdit,_text_changed_emit);

    BIND_ENUM_CONSTANT(SEARCH_MATCH_CASE);
    BIND_ENUM_CONSTANT(SEARCH_WHOLE_WORDS);
    BIND_ENUM_CONSTANT(SEARCH_BACKWARDS);

    BIND_ENUM_CONSTANT(SEARCH_RESULT_COLUMN);
    BIND_ENUM_CONSTANT(SEARCH_RESULT_LINE);
    /*
    BIND_METHOD(TextEdit,delete_char);
    BIND_METHOD(TextEdit,delete_line);
*/

    SE_BIND_METHOD(TextEdit,set_text);
    SE_BIND_METHOD(TextEdit,insert_text_at_cursor);

    SE_BIND_METHOD(TextEdit,get_line_count);
    SE_BIND_METHOD(TextEdit,get_text);
    SE_BIND_METHOD(TextEdit,get_line);
    SE_BIND_METHOD(TextEdit,set_line);
    //MethodBinder::bind_method(D_METHOD("get_line_wrapped_text", {"line"}), &TextEdit::get_wrap_rows_text);

    MethodBinder::bind_method(D_METHOD("get_line_width", {"line", "wrap_index"}), &TextEdit::get_line_width, {DEFVAL(-1)});
    SE_BIND_METHOD(TextEdit,get_line_height);

    SE_BIND_METHOD(TextEdit,is_line_wrapped);
    SE_BIND_METHOD(TextEdit,get_line_wrap_count);

    SE_BIND_METHOD(TextEdit,center_viewport_to_cursor);
    MethodBinder::bind_method(D_METHOD("cursor_set_column", { "column", "adjust_viewport" }),
            &TextEdit::cursor_set_column, { DEFVAL(true) });
    MethodBinder::bind_method(D_METHOD("cursor_set_line", { "line", "adjust_viewport", "can_be_hidden", "wrap_index" }),
            &TextEdit::cursor_set_line, { DEFVAL(true), DEFVAL(true), DEFVAL(0) });

    SE_BIND_METHOD(TextEdit,cursor_get_column);
    SE_BIND_METHOD(TextEdit,cursor_get_line);
    SE_BIND_METHOD(TextEdit,cursor_set_blink_enabled);
    SE_BIND_METHOD(TextEdit,cursor_get_blink_enabled);
    SE_BIND_METHOD(TextEdit,cursor_set_blink_speed);
    SE_BIND_METHOD(TextEdit,cursor_get_blink_speed);
    SE_BIND_METHOD(TextEdit,cursor_set_block_mode);
    SE_BIND_METHOD(TextEdit,cursor_is_block_mode);

    MethodBinder::bind_method(
            D_METHOD("set_right_click_moves_caret", { "enable" }), &TextEdit::set_right_click_moves_caret);
    SE_BIND_METHOD(TextEdit,is_right_click_moving_caret);
    /* Line and character position. */
    SE_BIND_METHOD(TextEdit,get_pos_at_line_column);
    SE_BIND_METHOD(TextEdit,get_rect_at_line_column);
    SE_BIND_METHOD(TextEdit,get_line_column_at_pos);

    SE_BIND_METHOD(TextEdit,set_readonly);
    SE_BIND_METHOD(TextEdit,is_readonly);

    SE_BIND_METHOD(TextEdit,set_wrap_enabled);
    SE_BIND_METHOD(TextEdit,is_wrap_enabled);
    SE_BIND_METHOD(TextEdit,set_context_menu_enabled);
    SE_BIND_METHOD(TextEdit,is_context_menu_enabled);
    MethodBinder::bind_method(
            D_METHOD("set_shortcut_keys_enabled", { "enable" }), &TextEdit::set_shortcut_keys_enabled);
    SE_BIND_METHOD(TextEdit,is_shortcut_keys_enabled);
    SE_BIND_METHOD(TextEdit,set_middle_mouse_paste_enabled);
    SE_BIND_METHOD(TextEdit,is_middle_mouse_paste_enabled);
    SE_BIND_METHOD(TextEdit,set_selecting_enabled);
    SE_BIND_METHOD(TextEdit,is_selecting_enabled);
    SE_BIND_METHOD(TextEdit,set_deselect_on_focus_loss_enabled);
    SE_BIND_METHOD(TextEdit,is_deselect_on_focus_loss_enabled);

    SE_BIND_METHOD(TextEdit,cut);
    SE_BIND_METHOD(TextEdit,copy);
    SE_BIND_METHOD(TextEdit,paste);

    MethodBinder::bind_method(
            D_METHOD("select", { "from_line", "from_column", "to_line", "to_column" }), &TextEdit::select);
    SE_BIND_METHOD(TextEdit,select_all);
    SE_BIND_METHOD(TextEdit,deselect);

    SE_BIND_METHOD(TextEdit,is_selection_active);
    SE_BIND_METHOD(TextEdit,get_selection_from_line);
    SE_BIND_METHOD(TextEdit,get_selection_from_column);
    SE_BIND_METHOD(TextEdit,get_selection_to_line);
    SE_BIND_METHOD(TextEdit,get_selection_to_column);
    SE_BIND_METHOD(TextEdit,get_selection_text);
    SE_BIND_METHOD(TextEdit,is_mouse_over_selection);
    SE_BIND_METHOD(TextEdit,get_word_under_cursor);
    MethodBinder::bind_method(
            D_METHOD("search", { "key", "flags", "from_line", "from_column" }), &TextEdit::_search_bind);

    SE_BIND_METHOD(TextEdit,has_undo);
    SE_BIND_METHOD(TextEdit,has_redo);
    SE_BIND_METHOD(TextEdit,undo);
    SE_BIND_METHOD(TextEdit,redo);
    SE_BIND_METHOD(TextEdit,clear_undo_history);

    SE_BIND_METHOD(TextEdit,set_show_line_numbers);
    SE_BIND_METHOD(TextEdit,is_show_line_numbers_enabled);
    SE_BIND_METHOD(TextEdit,set_draw_tabs);
    SE_BIND_METHOD(TextEdit,is_drawing_tabs);
    SE_BIND_METHOD(TextEdit,set_draw_spaces);
    SE_BIND_METHOD(TextEdit,is_drawing_spaces);
    SE_BIND_METHOD(TextEdit,set_bookmark_gutter_enabled);
    SE_BIND_METHOD(TextEdit,is_bookmark_gutter_enabled);
    MethodBinder::bind_method(
            D_METHOD("set_breakpoint_gutter_enabled", { "enable" }), &TextEdit::set_breakpoint_gutter_enabled);
    SE_BIND_METHOD(TextEdit,is_breakpoint_gutter_enabled);
    SE_BIND_METHOD(TextEdit,set_draw_fold_gutter);
    SE_BIND_METHOD(TextEdit,is_drawing_fold_gutter);
    SE_BIND_METHOD(TextEdit,get_total_gutter_width);
    SE_BIND_METHOD(TextEdit,get_visible_rows);
    SE_BIND_METHOD(TextEdit,get_total_visible_rows);

    SE_BIND_METHOD(TextEdit,set_hiding_enabled);
    SE_BIND_METHOD(TextEdit,is_hiding_enabled);
    SE_BIND_METHOD(TextEdit,set_line_as_hidden);
    SE_BIND_METHOD(TextEdit,is_line_hidden);
    SE_BIND_METHOD(TextEdit,fold_all_lines);
    SE_BIND_METHOD(TextEdit,unhide_all_lines);
    SE_BIND_METHOD(TextEdit,fold_line);
    SE_BIND_METHOD(TextEdit,unfold_line);
    SE_BIND_METHOD(TextEdit,toggle_fold_line);
    SE_BIND_METHOD(TextEdit,can_fold);
    SE_BIND_METHOD(TextEdit,is_folded);

    MethodBinder::bind_method(
            D_METHOD("set_highlight_all_occurrences", { "enable" }), &TextEdit::set_highlight_all_occurrences);
    MethodBinder::bind_method(
            D_METHOD("is_highlight_all_occurrences_enabled"), &TextEdit::is_highlight_all_occurrences_enabled);

    MethodBinder::bind_method(
            D_METHOD("set_override_selected_font_color", { "override" }), &TextEdit::set_override_selected_font_color);
    MethodBinder::bind_method(
            D_METHOD("is_overriding_selected_font_color"), &TextEdit::is_overriding_selected_font_color);

    SE_BIND_METHOD(TextEdit,set_syntax_coloring);
    SE_BIND_METHOD(TextEdit,is_syntax_coloring_enabled);

    MethodBinder::bind_method(
            D_METHOD("set_highlight_current_line", { "enabled" }), &TextEdit::set_highlight_current_line);
    MethodBinder::bind_method(
            D_METHOD("is_highlight_current_line_enabled"), &TextEdit::is_highlight_current_line_enabled);

    MethodBinder::bind_method(
            D_METHOD("set_smooth_scroll_enabled", { "enable" }), &TextEdit::set_smooth_scroll_enabled);
    SE_BIND_METHOD(TextEdit,is_smooth_scroll_enabled);
    SE_BIND_METHOD(TextEdit,set_v_scroll_speed);
    SE_BIND_METHOD(TextEdit,get_v_scroll_speed);
    SE_BIND_METHOD(TextEdit,set_v_scroll);
    SE_BIND_METHOD(TextEdit,get_v_scroll);
    SE_BIND_METHOD(TextEdit,set_h_scroll);
    SE_BIND_METHOD(TextEdit,get_h_scroll);


    SE_BIND_METHOD(TextEdit,add_keyword_color);
    SE_BIND_METHOD(TextEdit,has_keyword_color);
    SE_BIND_METHOD(TextEdit,get_keyword_color);
    MethodBinder::bind_method(D_METHOD("add_color_region", { "begin_key", "end_key", "color", "line_only" }),
            &TextEdit::add_color_region, { DEFVAL(false) });
    SE_BIND_METHOD(TextEdit,clear_colors);
    SE_BIND_METHOD(TextEdit,menu_option);
    SE_BIND_METHOD(TextEdit,get_menu);

    MethodBinder::bind_method(D_METHOD("get_breakpoints"), &TextEdit::get_breakpoints_array);
    SE_BIND_METHOD(TextEdit,remove_breakpoints);

    SE_BIND_METHOD(TextEdit,set_draw_minimap);
    SE_BIND_METHOD(TextEdit,is_drawing_minimap);
    SE_BIND_METHOD(TextEdit,set_minimap_width);
    SE_BIND_METHOD(TextEdit,get_minimap_width);
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "text", PropertyHint::MultilineText), "set_text", "get_text");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "readonly"), "set_readonly", "is_readonly");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "highlight_current_line"), "set_highlight_current_line",
            "is_highlight_current_line_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "syntax_highlighting"), "set_syntax_coloring",
            "is_syntax_coloring_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "show_line_numbers"), "set_show_line_numbers",
            "is_show_line_numbers_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "draw_tabs"), "set_draw_tabs", "is_drawing_tabs");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "draw_spaces"), "set_draw_spaces", "is_drawing_spaces");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "bookmark_gutter"), "set_bookmark_gutter_enabled", "is_bookmark_gutter_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "breakpoint_gutter"), "set_breakpoint_gutter_enabled",
            "is_breakpoint_gutter_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "fold_gutter"), "set_draw_fold_gutter", "is_drawing_fold_gutter");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "highlight_all_occurrences"), "set_highlight_all_occurrences",
            "is_highlight_all_occurrences_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "override_selected_font_color"), "set_override_selected_font_color",
            "is_overriding_selected_font_color");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "context_menu_enabled"), "set_context_menu_enabled",
            "is_context_menu_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "shortcut_keys_enabled"), "set_shortcut_keys_enabled",
            "is_shortcut_keys_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "middle_mouse_paste_enabled"), "set_middle_mouse_paste_enabled", "is_middle_mouse_paste_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "selecting_enabled"), "set_selecting_enabled", "is_selecting_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "deselect_on_focus_loss_enabled"), "set_deselect_on_focus_loss_enabled", "is_deselect_on_focus_loss_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "smooth_scrolling"), "set_smooth_scroll_enabled",
            "is_smooth_scroll_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "v_scroll_speed"), "set_v_scroll_speed", "get_v_scroll_speed");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "hiding_enabled"), "set_hiding_enabled", "is_hiding_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "wrap_enabled"), "set_wrap_enabled", "is_wrap_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "scroll_vertical"), "set_v_scroll", "get_v_scroll");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "scroll_horizontal"), "set_h_scroll", "get_h_scroll");

    ADD_GROUP("Minimap", "minimap_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "minimap_draw"), "set_draw_minimap", "is_drawing_minimap");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "minimap_width"), "set_minimap_width", "get_minimap_width");

    ADD_GROUP("Caret", "caret_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "caret_block_mode"), "cursor_set_block_mode", "cursor_is_block_mode");
    ADD_PROPERTY(
            PropertyInfo(VariantType::BOOL, "caret_blink"), "cursor_set_blink_enabled", "cursor_get_blink_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "caret_blink_speed", PropertyHint::Range, "0.1,10,0.01"),
            "cursor_set_blink_speed", "cursor_get_blink_speed");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "caret_moving_by_right_click"), "set_right_click_moves_caret",
            "is_right_click_moving_caret");

    ADD_SIGNAL(MethodInfo("cursor_changed"));
    ADD_SIGNAL(MethodInfo("text_changed"));
    ADD_SIGNAL(MethodInfo("request_completion"));
    ADD_SIGNAL(MethodInfo("breakpoint_toggled", PropertyInfo(VariantType::INT, "row")));
    ADD_SIGNAL(MethodInfo("symbol_lookup", PropertyInfo(VariantType::STRING, "symbol"),
            PropertyInfo(VariantType::INT, "row"), PropertyInfo(VariantType::INT, "column")));
    ADD_SIGNAL(MethodInfo(
            "info_clicked", PropertyInfo(VariantType::INT, "row"), PropertyInfo(VariantType::STRING, "info")));

    BIND_ENUM_CONSTANT(MENU_CUT);
    BIND_ENUM_CONSTANT(MENU_COPY);
    BIND_ENUM_CONSTANT(MENU_PASTE);
    BIND_ENUM_CONSTANT(MENU_CLEAR);
    BIND_ENUM_CONSTANT(MENU_SELECT_ALL);
    BIND_ENUM_CONSTANT(MENU_UNDO);
    BIND_ENUM_CONSTANT(MENU_REDO);
    BIND_ENUM_CONSTANT(MENU_MAX);

    GLOBAL_DEF("gui/timers/text_edit_idle_detect_sec", 3);
    ProjectSettings::get_singleton()->set_custom_property_info("gui/timers/text_edit_idle_detect_sec",
            PropertyInfo(VariantType::FLOAT, "gui/timers/text_edit_idle_detect_sec", PropertyHint::Range,
                    "0,10,0.01,or_greater")); // No negative numbers.
    GLOBAL_DEF("gui/common/text_edit_undo_stack_max_size", 1024);
    ProjectSettings::get_singleton()->set_custom_property_info("gui/common/text_edit_undo_stack_max_size",
            PropertyInfo(VariantType::INT, "gui/common/text_edit_undo_stack_max_size", PropertyHint::Range,
                    "0,10000,1,or_greater")); // No negative numbers.

}

TextEdit::TextEdit() {

    indent_size = 4;
    m_priv = new PrivateData(this,indent_size);
    clear();
    set_focus_mode(FOCUS_ALL);
    D()->syntax_highlighter = nullptr;
    _update_caches();
    D()->cache.row_height = 1;
    D()->cache.line_spacing = 1;
    D()->cache.line_number_w = 1;
    D()->cache.breakpoint_gutter_width = 0;
    breakpoint_gutter_width = 0;
    D()->cache.fold_gutter_width = 0;
    fold_gutter_width = 0;
    info_gutter_width = 0;
    D()->cache.info_gutter_width = 0;
    set_default_cursor_shape(CURSOR_IBEAM);


    h_scroll = memnew(HScrollBar);
    v_scroll = memnew(VScrollBar);

    add_child(h_scroll);
    add_child(v_scroll);

    updating_scrolls = false;


    h_scroll->connect("value_changed",callable_mp(this, &ClassName::_scroll_moved));
    v_scroll->connect("value_changed",callable_mp(this, &ClassName::_scroll_moved));

    v_scroll->connect("scrolling",callable_mp(this, &ClassName::_v_scroll_input));

    syntax_coloring = false;

    block_caret = false;
    caret_blink_enabled = false;
    caret_blink_timer = memnew(Timer);
    add_child(caret_blink_timer);
    caret_blink_timer->set_wait_time(0.65);
    caret_blink_timer->connect("timeout",callable_mp(this, &ClassName::_toggle_draw_caret));
    cursor_set_blink_enabled(false);
    right_click_moves_caret = true;

    idle_detect = memnew(Timer);
    add_child(idle_detect);
    idle_detect->set_one_shot(true);
    idle_detect->set_wait_time(T_GLOBAL_GET<float>("gui/timers/text_edit_idle_detect_sec"));
    idle_detect->connect("timeout",callable_mp(this, &ClassName::_push_current_op));

    D()->click_select_held = memnew(Timer);
    add_child(D()->click_select_held);
    D()->click_select_held->set_wait_time(0.05f);
    D()->click_select_held->connect("timeout",callable_mp(this, &ClassName::_click_selection_held));

    last_dblclk = 0;

    tooltip_obj_id = entt::null;
    line_numbers = false;
    line_numbers_zero_padded = false;
    line_length_guidelines = false;
    line_length_guideline_soft_col = 80;
    line_length_guideline_hard_col = 100;
    draw_bookmark_gutter = false;
    draw_breakpoint_gutter = false;
    draw_fold_gutter = false;
    draw_info_gutter = false;
    scroll_past_end_of_file_enabled = false;
    auto_brace_completion_enabled = false;
    brace_matching_enabled = false;
    highlight_all_occurrences = false;
    highlight_current_line = false;
    indent_using_spaces = false;
    auto_indent = false;
    insert_mode = false;
    window_has_focus = true;
    select_identifiers_enabled = false;
    smooth_scroll_enabled = false;
    scrolling = false;
    minimap_clicked = false;
    dragging_minimap = false;
    can_drag_minimap = false;
    minimap_scroll_ratio = 0;
    minimap_scroll_click_pos = 0;
    target_v_scroll = 0;
    v_scroll_speed = 80;
    draw_minimap = false;
    minimap_width = 80;
    minimap_char_size = Point2(1, 2);
    minimap_line_spacing = 1;

    context_menu_enabled = true;
    shortcut_keys_enabled = true;
    menu = memnew(PopupMenu);
    add_child(menu);
    readonly = true; // Initialise to opposite first, so we get past the early-out in set_readonly.
    set_readonly(false);
    menu->connect("id_pressed",callable_mp(this, &ClassName::menu_option));
    first_draw = true;

    executing_line = -1;
}

TextEdit::~TextEdit() {
    delete D();
    m_priv = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

Map<int, TextEdit::HighlighterInfo> PrivateData::_get_line_syntax_highlighting(TextEdit *te,int p_line) {
    if (syntax_highlighting_cache.contains(p_line)) {
        return syntax_highlighting_cache[p_line];
    }

    if (syntax_highlighter != nullptr) {
        Map<int, TextEdit::HighlighterInfo> color_map = syntax_highlighter->_get_line_syntax_highlighting(p_line);
        syntax_highlighting_cache[p_line] = color_map;
        return color_map;
    }

    Map<int, TextEdit::HighlighterInfo> color_map;

    bool prev_is_char = false;
    bool prev_is_number = false;
    bool in_keyword = false;
    bool in_word = false;
    bool in_function_name = false;
    bool in_member_variable = false;
    bool is_hex_notation = false;
    Color keyword_color;
    Color color;

    int in_region = te->_is_line_in_region(p_line);
    int deregion = 0;

    const Map<int, TextColorRegionInfo> cri_map = text.get_color_region_info(p_line);
    const UIString &str = text[p_line];
    Color prev_color;
    for (int j = 0; j < str.length(); j++) {
        TextEdit::HighlighterInfo highlighter_info;

        if (deregion > 0) {
            deregion--;
            if (deregion == 0) {
                in_region = -1;
            }
        }

        if (deregion != 0) {
            if (color != prev_color) {
                prev_color = color;
                highlighter_info.color = color;
                color_map[j] = highlighter_info;
            }
            continue;
        }

        color = cache.font_color;

        bool is_char = _te_is_text_char(str[j]);
        bool is_symbol = _is_symbol(str[j]);
        bool is_number = _is_number(str[j]);

        // Allow ABCDEF in hex notation.
        if (is_hex_notation && (_is_hex_symbol(str[j]) || is_number)) {
            is_number = true;
        } else {
            is_hex_notation = false;
        }

        // Check for dot or underscore or 'x' for hex notation in floating point number or 'e' for scientific notation.
        if ((str[j] == '.' || str[j] == 'x' || str[j] == '_' || str[j] == 'f' || str[j] == 'e') && !in_word &&
                prev_is_number && !is_number) {
            is_number = true;
            is_symbol = false;
            is_char = false;

            if (str[j] == 'x' && str[j - 1] == '0') {
                is_hex_notation = true;
            }
        }

        if (!in_word && _is_char(str[j]) && !is_number) {
            in_word = true;
        }

        if ((in_keyword || in_word) && !is_hex_notation) {
            is_number = false;
        }

        if (is_symbol && str[j] != '.' && in_word) {
            in_word = false;
        }

        if (is_symbol && cri_map.contains(j)) {
            const TextColorRegionInfo &cri = cri_map.at(j);

            if (in_region == -1) {
                if (!cri.end) {
                    in_region = cri.region;
                }
            } else if (in_region == cri.region && !color_regions[cri.region].line_only) { // Ignore otherwise.
                if (cri.end || color_regions[cri.region].eq) {
                    deregion = color_regions[cri.region].eq ? color_regions[cri.region].begin_key.length() :
                                                              color_regions[cri.region].end_key.length();
                }
            }
        }

        if (!is_char) {
            in_keyword = false;
        }

        if (in_region == -1 && !in_keyword && is_char && !prev_is_char) {

            int to = j;
            while (to < str.length() && _te_is_text_char(str[to]))
                to++;

            //uint32_t hash = StringUtils::hash(str.constData()+j, to - j);

            QStringRef range(str.midRef(j, to - j));
            auto iter = keywords.find_as(range);
            const Color *col = iter!=keywords.end() ? &iter->second : nullptr;

            if (!col) {
                auto iter=member_keywords.find_as(range);
                col = iter!=member_keywords.end() ? &iter->second : nullptr;

                if (col) {
                    for (int k = j - 1; k >= 0; k--) {
                        if (str[k] == '.') {
                            col = nullptr; // Member indexing not allowed.
                            break;
                        } else if (str[k] > 32) {
                            break;
                        }
                    }
                }
            }

            if (col) {
                in_keyword = true;
                keyword_color = *col;
            }
        }

        if (!in_function_name && in_word && !in_keyword) {

            int k = j;
            while (k < str.length() && !_is_symbol(str[k]) && str[k] != '\t' && str[k] != ' ') {
                k++;
            }

            // Check for space between name and bracket.
            while (k < str.length() && (str[k] == '\t' || str[k] == ' ')) {
                k++;
            }

            if (k<str.length() && str[k] == '(') {
                in_function_name = true;
            }
        }

        if (!in_function_name && !in_member_variable && !in_keyword && !is_number && in_word) {
            int k = j;
            while (k > 0 && !_is_symbol(str[k]) && str[k] != '\t' && str[k] != ' ') {
                k--;
            }

            if (str[k] == '.') {
                in_member_variable = true;
            }
        }

        if (is_symbol) {
            in_function_name = false;
            in_member_variable = false;
        }

        if (in_region >= 0)
            color = color_regions[in_region].color;
        else if (in_keyword)
            color = keyword_color;
        else if (in_member_variable)
            color = cache.member_variable_color;
        else if (in_function_name)
            color = cache.function_color;
        else if (is_symbol)
            color = cache.symbol_color;
        else if (is_number)
            color = cache.number_color;

        prev_is_char = is_char;
        prev_is_number = is_number;

        if (color != prev_color) {
            prev_color = color;
            highlighter_info.color = color;
            color_map[j] = highlighter_info;
        }
    }

    syntax_highlighting_cache[p_line] = color_map;
    return color_map;
}

void SyntaxHighlighter::set_text_editor(TextEdit *p_text_editor) {
    text_editor = p_text_editor;
}

TextEdit *SyntaxHighlighter::get_text_editor() {
    return text_editor;
}
#undef D
