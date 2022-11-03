/*************************************************************************/
/*  popup_menu.h                                                         */
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

#include "scene/gui/popup.h"
#include "scene/gui/shortcut.h"
#include "core/string.h"

class GODOT_EXPORT PopupMenu : public Popup {

    GDCLASS(PopupMenu,Popup)

    struct Item {
        Ref<Texture> icon;
        String text;
        String xl_text;
        enum {
            CHECKABLE_TYPE_NONE,
            CHECKABLE_TYPE_CHECK_BOX,
            CHECKABLE_TYPE_RADIO_BUTTON,
        } checkable_type;
        int max_states;
        int state;
        int id;
        Variant metadata;
        StringName submenu;
        String tooltip;
        uint32_t accel;
        int _ofs_cache;
        int h_ofs;
        Ref<ShortCut> shortcut;
        bool checked;
        bool separator;
        bool disabled;
        bool shortcut_is_global;
        bool shortcut_is_disabled;

        Item() {
            checked = false;
            checkable_type = CHECKABLE_TYPE_NONE;
            separator = false;
            max_states = 0;
            state = 0;
            accel = 0;
            disabled = false;
            _ofs_cache = 0;
            h_ofs = 0;
            shortcut_is_global = false;
            shortcut_is_disabled = false;
        }
    };

    Vector<Rect2> autohide_areas;
    Vector<Item> items;
    HashMap<Ref<ShortCut>, int> shortcut_refcount;
    Rect2 parent_rect;
    Vector2 moved;
    String search_string;
    class Timer *submenu_timer;
    uint64_t search_time_msec;
    int initial_button_mask;
    int mouse_over;
    int submenu_over;
    bool during_grabbed_click;
    bool invalidated_click;
    bool hide_on_item_selection;
    bool hide_on_checkable_item_selection;
    bool hide_on_multistate_item_selection;
    bool hide_on_window_lose_focus;
    bool allow_search;
    String _get_accel_text(int p_item) const;
    int _get_mouse_over(const Point2 &p_over) const;
    Size2 get_minimum_size() const override;
    void _scroll(float p_factor, const Point2 &p_over);
    void _gui_input(const Ref<InputEvent> &p_event);
    void _activate_submenu(int over);
    void _submenu_timeout();

public:
    Array _get_items() const;
    void _set_items(const Array &p_items);
    void _ref_shortcut(Ref<ShortCut> p_sc);
    void _unref_shortcut(Ref<ShortCut> p_sc);


protected:
    bool has_point(const Point2 &p_point) const override;
    void perform_draw();

    friend class MenuButton;
    void _notification(int p_what);
    static void _bind_methods();

public:
    void add_item(const StringName &p_label, int p_id = -1, uint32_t p_accel = 0);
    void add_icon_item(const Ref<Texture> &p_icon, const StringName &p_label, int p_id = -1, uint32_t p_accel = 0);
    void add_icon_item_utf8(const Ref<Texture> &p_icon, StringView _label, int p_id = -1, uint32_t p_accel = 0);
    void add_check_item(const StringName &p_label, int p_id = -1, uint32_t p_accel = 0);
    void add_check_item_utf8(StringView p_label, int p_id = -1, uint32_t p_accel = 0);
    void add_icon_check_item(const Ref<Texture> &p_icon, const StringName &p_label, int p_id = -1, uint32_t p_accel = 0);
    void add_radio_check_item_utf8(StringView p_label, int p_id = -1, uint32_t p_accel = 0);
    void add_radio_check_item(StringView p_label, int p_id = -1, uint32_t p_accel = 0);
    void add_icon_radio_check_item(const Ref<Texture> &p_icon, const StringName &p_label, int p_id = -1, uint32_t p_accel = 0);

    void add_multistate_item(const StringName &p_label, int p_max_states, int p_default_state = 0, int p_id = -1, uint32_t p_accel = 0);

    void add_shortcut(const Ref<ShortCut> &p_shortcut, int p_id = -1, bool p_global = false);
    void add_icon_shortcut(const Ref<Texture> &p_icon, const Ref<ShortCut> &p_shortcut, int p_id = -1, bool p_global = false);
    void add_check_shortcut(const Ref<ShortCut> &p_shortcut, int p_id = -1, bool p_global = false);
    void add_icon_check_shortcut(const Ref<Texture> &p_icon, const Ref<ShortCut> &p_shortcut, int p_id = -1, bool p_global = false);
    void add_radio_check_shortcut(const Ref<ShortCut> &p_shortcut, int p_id = -1, bool p_global = false);
    void add_icon_radio_check_shortcut(const Ref<Texture> &p_icon, const Ref<ShortCut> &p_shortcut, int p_id = -1, bool p_global = false);

    void add_submenu_item(const StringName &p_label, const StringName &p_submenu, int p_id = -1);

    void set_item_text(int p_idx, const StringName &p_text);
    void set_item_icon(int p_idx, const Ref<Texture> &p_icon);
    void set_item_checked(int p_idx, bool p_checked);
    void set_item_id(int p_idx, int p_id);
    void set_item_accelerator(int p_idx, uint32_t p_accel);
    void set_item_metadata(int p_idx, const Variant &p_meta);
    void set_item_disabled(int p_idx, bool p_disabled);
    void set_item_submenu(int p_idx, const StringName &p_submenu);
    void set_item_as_separator(int p_idx, bool p_separator);
    void set_item_as_checkable(int p_idx, bool p_checkable);
    void set_item_as_radio_checkable(int p_idx, bool p_radio_checkable);
    void set_item_tooltip(int p_idx, const StringName &p_tooltip);
    void set_item_shortcut(int p_idx, const Ref<ShortCut> &p_shortcut, bool p_global = false);
    void set_item_h_offset(int p_idx, int p_offset);
    void set_item_multistate(int p_idx, int p_state);
    void toggle_item_multistate(int p_idx);
    void set_item_shortcut_disabled(int p_idx, bool p_disabled);

    void toggle_item_checked(int p_idx);

    const String &get_item_text(int p_idx) const;
    int get_item_idx_from_text(const StringName &text) const;
    int get_item_idx_from_text_utf8(StringView text) const;
    Ref<Texture> get_item_icon(int p_idx) const;
    bool is_item_checked(int p_idx) const;
    int get_item_id(int p_idx) const;
    int get_item_index(int p_id) const;
    uint32_t get_item_accelerator(int p_idx) const;
    Variant get_item_metadata(int p_idx) const;
    bool is_item_disabled(int p_idx) const;
    StringName get_item_submenu(int p_idx) const;
    bool is_item_separator(int p_idx) const;
    bool is_item_checkable(int p_idx) const;
    bool is_item_radio_checkable(int p_idx) const;
    bool is_item_shortcut_disabled(int p_idx) const;
    const String &get_item_tooltip(int p_idx) const;
    Ref<ShortCut> get_item_shortcut(int p_idx) const;
    int get_item_state(int p_idx) const;

    int get_current_index() const;
    void set_current_index(int p_idx);
    int get_item_count() const;

    bool activate_item_by_event(const Ref<InputEvent> &p_event, bool p_for_global_only = false);
    void activate_item(int p_item);

    void remove_item(int p_idx);

    void add_separator(const StringName &p_text = StringName(), int p_id = -1);

    void clear();

    void set_parent_rect(const Rect2 &p_rect);

    const String & get_tooltip(const Point2 &p_pos) const override;

    void get_translatable_strings(List<String> *p_strings) const override;

    void add_autohide_area(const Rect2 &p_area);
    void clear_autohide_areas();

    void set_hide_on_item_selection(bool p_enabled);
    bool is_hide_on_item_selection() const;

    void set_hide_on_checkable_item_selection(bool p_enabled);
    bool is_hide_on_checkable_item_selection() const;

    void set_hide_on_multistate_item_selection(bool p_enabled);
    bool is_hide_on_multistate_item_selection() const;

    void set_submenu_popup_delay(float p_time);
    float get_submenu_popup_delay() const;

    void set_allow_search(bool p_allow);
    bool get_allow_search() const;

    void popup(const Rect2 &p_bounds = Rect2()) override;

    void set_hide_on_window_lose_focus(bool p_enabled);
    bool is_hide_on_window_lose_focus() const;

    PopupMenu();
    ~PopupMenu() override;
};
