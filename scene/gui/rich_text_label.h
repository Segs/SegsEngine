/*************************************************************************/
/*  rich_text_label.h                                                    */
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

#include "rich_text_effect.h"
#include "scene/gui/scroll_bar.h"
#include "core/string.h"

class InputEventMouseButton;
struct RichTextItem;
struct RichTextItemFrame;
struct RichTextItemMeta;
struct RichTextItemFX;

class GODOT_EXPORT RichTextLabel : public Control {

    GDCLASS(RichTextLabel,Control)

public:
    enum Align {

        ALIGN_LEFT,
        ALIGN_CENTER,
        ALIGN_RIGHT,
        ALIGN_FILL
    };
    enum InlineAlign {

        INLINE_ALIGN_TOP,
        INLINE_ALIGN_CENTER,
        INLINE_ALIGN_BASELINE,
        INLINE_ALIGN_BOTTOM
    };
    enum ListType {

        LIST_NUMBERS,
        LIST_LETTERS,
        LIST_DOTS
    };

    enum ItemType {

        ITEM_FRAME,
        ITEM_TEXT,
        ITEM_IMAGE,
        ITEM_NEWLINE,
        ITEM_FONT,
        ITEM_COLOR,
        ITEM_UNDERLINE,
        ITEM_STRIKETHROUGH,
        ITEM_ALIGN,
        ITEM_INDENT,
        ITEM_LIST,
        ITEM_TABLE,
        ITEM_FADE,
        ITEM_SHAKE,
        ITEM_WAVE,
        ITEM_TORNADO,
        ITEM_RAINBOW,
        ITEM_META,
        ITEM_CUSTOMFX,
        ITEM_TYPE_MAX,
    };

protected:
    static void _bind_methods();

private:

    RichTextItemFrame *main;
    RichTextItem *current;
    RichTextItemFrame *current_frame;

    VScrollBar *vscroll;

    bool scroll_visible;
    bool scroll_follow;
    bool scroll_following;
    bool scroll_active;
    int scroll_w;
    bool scroll_updated;
    bool updating_scroll;
    int current_idx;
    int visible_line_count;

    int tab_size;
    bool underline_meta;
    bool override_selected_font_color;

    Align default_align;

    RichTextItemMeta *meta_hovering;
    Variant current_meta;

    Vector<Ref<RichTextEffect> > custom_effects;

    void _invalidate_current_line(RichTextItemFrame *p_frame);
    void _validate_line_caches(RichTextItemFrame *p_frame);

    void _add_item(RichTextItem *p_item, bool p_enter = false, bool p_ensure_newline = false);
    void _remove_item(RichTextItem *p_item, const int p_line, const int p_subitem_line);

    struct ProcessState {

        int line_width;
    };

    enum ProcessMode {

        PROCESS_CACHE,
        PROCESS_DRAW,
        PROCESS_POINTER
    };

    struct Selection {

        RichTextItem *click;
        RichTextItem *from;
        RichTextItem *to;
        int click_char;

        int from_char;
        int to_char;

        bool active; // anything selected? i.e. from, to, etc. valid?
        bool enabled; // allow selections?
        bool drag_attempt;
    };

    Selection selection;

    int visible_characters;
    float percent_visible;
    bool use_bbcode;
    bool deselect_on_focus_loss_enabled;
    String bbcode;
    int fixed_width;
    bool fit_content_height; 

    bool _is_click_inside_selection() const;
    int _process_line(RichTextItemFrame *p_frame, const Vector2 &p_ofs, int &y, int p_width, int p_line, ProcessMode p_mode, const Ref<Font> &p_base_font, const Color &p_base_color, const Color &p_font_color_shadow, bool p_shadow_as_outline, const Point2 &shadow_ofs, const Point2i &p_click_pos = Point2i(), RichTextItem **r_click_item = nullptr, int *r_click_char = nullptr, bool *r_outside = nullptr, int p_char_count = 0);
    void _find_click(RichTextItemFrame *p_frame, const Point2i &p_click, RichTextItem **r_click_item = nullptr, int *r_click_char = nullptr, bool *r_outside = nullptr);

    Ref<Font> _find_font(RichTextItem *p_item);
    int _find_margin(RichTextItem *p_item, const Ref<Font> &p_base_font);
    Align _find_align(RichTextItem *p_item);
    Color _find_color(RichTextItem *p_item, const Color &p_default_color);
    bool _find_underline(RichTextItem *p_item);
    bool _find_strikethrough(RichTextItem *p_item);
    bool _find_meta(RichTextItem *p_item, Variant *r_meta, RichTextItemMeta **r_item = nullptr);
    bool _find_layout_subitem(RichTextItem *from, RichTextItem *to);
    bool _find_by_type(RichTextItem *p_item, ItemType p_type);

    void _fetch_item_fx_stack(RichTextItem *p_item, Vector<RichTextItemFX *> &r_stack);

    void _update_scroll();
    void _update_fx(RichTextItemFrame *p_frame, float p_delta_time);
    void _scroll_changed(double);

    void _gui_input(const Ref<InputEvent>& p_event);
    RichTextItem *_get_next_item(RichTextItem *p_item, bool p_free = false);
    RichTextItem *_get_prev_item(RichTextItem *p_item, bool p_free = false);
    bool handle_mouse_button(const InputEventMouseButton *b);

    Rect2 _get_text_rect();
    void process_line_text_item(RichTextItemFrame *p_frame, const Vector2 &p_ofs, int &y, int p_width, int p_line,
            ProcessMode p_mode, const Ref<Font> &p_base_font, const Color &p_base_color,
            const Color &p_font_color_shadow, bool p_shadow_as_outline, const Point2 &shadow_ofs,
            const Point2i &p_click_pos, RichTextItem **r_click_item, int *r_click_char, bool *r_outside,
            int p_char_count);
    Ref<RichTextEffect> _get_custom_effect_by_code(StringView p_bbcode_identifier);


public:
    virtual Dictionary parse_expressions_for_values(const PoolVector<String> &p_expressions);

protected:
    void _notification(int p_what);

public:
    String get_text();
    void add_text(StringView p_text);
    void add_text_uistring(const UIString &p_text);
    void add_image(const Ref<Texture> &p_image, const int p_width = 0, const int p_height = 0, InlineAlign p_align = INLINE_ALIGN_BASELINE);
    void add_newline();
    bool remove_line(const int p_line);
    void push_font(const Ref<Font> &p_font);
    void push_normal();
    void push_bold();
    void push_bold_italics();
    void push_italics();
    void push_mono();
    void push_color(const Color &p_color);
    void push_underline();
    void push_strikethrough();
    void push_align(Align p_align);
    void push_indent(int p_level);
    void push_list(ListType p_list);
    void push_meta(const Variant &p_meta);
    void push_table(int p_columns);
    void push_fade(int p_start_index, int p_length);
    void push_shake(int p_strength, float p_rate);
    void push_wave(float p_frequency, float p_amplitude);
    void push_tornado(float p_frequency, float p_radius);
    void push_rainbow(float p_saturation, float p_value, float p_frequency);
    void push_customfx(const Ref<RichTextEffect> &p_custom_effect, Dictionary p_environment);
    void set_table_column_expand(int p_column, bool p_expand, int p_ratio = 1);
    int get_current_table_column() const;
    void push_cell();
    void pop();

    void clear();

    void set_offset(int p_pixel);

    void set_meta_underline(bool p_underline);
    bool is_meta_underlined() const;

    void set_override_selected_font_color(bool p_override_selected_font_color);
    bool is_overriding_selected_font_color() const;

    void set_scroll_active(bool p_active);
    bool is_scroll_active() const;

    void set_scroll_follow(bool p_follow);
    bool is_scroll_following() const;

    void set_tab_size(int p_spaces);
    int get_tab_size() const;

    bool search(const UIString &p_string, bool p_from_selection = false, bool p_search_previous = false);

    void scroll_to_line(int p_line);
    int get_line_count() const;
    int get_visible_line_count() const;

    int get_content_height();

    VScrollBar *get_v_scroll() { return vscroll; }

    CursorShape get_cursor_shape(const Point2 &p_pos) const override;
    Variant get_drag_data(const Point2 &p_point) override;

    void set_selection_enabled(bool p_enabled);
    bool is_selection_enabled() const;
    String get_selected_text();
    void selection_copy();
    void set_deselect_on_focus_loss_enabled(bool p_enabled);
    bool is_deselect_on_focus_loss_enabled() const;
    void deselect();

    Error parse_bbcode(StringView p_bbcode);
    Error append_bbcode(StringView p_bbcode);

    void set_use_bbcode(bool p_enable);
    bool is_using_bbcode() const;

    void set_bbcode(StringView p_bbcode);
    const String &get_bbcode() const;

    void set_text(StringView p_string);
    void set_text_ui(const UIString &p_string);

    void set_visible_characters(int p_visible);
    int get_visible_characters() const;
    int get_total_character_count() const;

    void set_percent_visible(float p_percent);
    float get_percent_visible() const;

    void set_effects(const Vector<Variant> &effects);
    Vector<Variant> get_effects();

    void install_effect(const Variant& effect);

    void set_fixed_size_to_width(int p_width);
    Size2 get_minimum_size() const override;

    RichTextLabel();
    ~RichTextLabel() override;
};
