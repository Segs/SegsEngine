/*************************************************************************/
/*  editor_inspector.h                                                   */
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

#include "core/string.h"
#include "core/vector.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/scroll_container.h"
#include "editor/editor_property_name_processor.h"

class UndoRedo;

class EditorPropertyRevert {
public:
    static bool get_instanced_node_original_property(
            Node *p_node, const StringName &p_prop, Variant &value, bool p_check_class_default = true);
    static bool is_node_property_different(Node *p_node, const Variant &p_current, const Variant &p_orig);

    static bool can_property_revert(Object *p_object, const StringName &p_property);
};

class GODOT_EXPORT EditorProperty : public Container {

    GDCLASS(EditorProperty,Container)
public:
    enum MenuItems {
        MENU_PIN_VALUE,
        MENU_COPY_PROPERTY,
        MENU_PASTE_PROPERTY,
        MENU_COPY_PROPERTY_PATH,
    };

private:
    String label;
    int text_size;
    friend class EditorInspector;
    Object *object = nullptr;
    StringName property;
    String property_path;

    Rect2 right_child_rect;
    Rect2 bottom_child_rect;
    Rect2 keying_rect;
    Rect2 revert_rect;
    Rect2 check_rect;
    int selected_focusable;
    float split_ratio = 0.5f;
    Vector<Control *> focusables;
    Control *label_reference;
    Control *bottom_editor;
    PopupMenu *menu;

    mutable String tooltip_text;
    int property_usage;

    bool read_only;
    bool checkable;
    bool checked;
    bool draw_red;
    bool keying;

    bool keying_hover;
    bool revert_hover;
    bool check_hover;

    bool can_revert;

    bool can_pin;
    bool pin_hidden;
    bool is_pinned;
    bool use_folding;
    bool draw_top_bg = true;

    bool selectable;
    bool selected;

	void _update_popup();
    void _menu_option(int p_option);
    void _focusable_focused(int p_index);
    void _update_pin_flags();

protected:
    void _notification(int p_what);
    static void _bind_methods();

    void _gui_input(const Ref<InputEvent> &p_event);
    void _unhandled_key_input(const Ref<InputEvent> &p_event);

public:
    void emit_changed(const StringName &p_property, const Variant &p_value, const StringName &p_field = StringName(),
            bool p_changing = false);

    Size2 get_minimum_size() const override;

    void set_label(StringView p_label);
    const String &get_label() const;

    void set_read_only(bool p_read_only);
    bool is_read_only() const;

    Object *get_edited_object();
    StringName get_edited_property();

    virtual void update_property();
    void update_revert_and_pin_status();

    virtual bool use_keying_next() const;

    void set_checkable(bool p_checkable);
    bool is_checkable() const { return checkable; }

    void set_checked(bool p_checked);
    bool is_checked() const { return checked; }

    void set_draw_red(bool p_draw_red);
    bool is_draw_red() const;

    void set_keying(bool p_keying);
    bool is_keying() const;

    void add_focusable(Control *p_control);
    void select(int p_focusable = -1);
    void deselect();
    bool is_selected() const { return selected; }

    void set_label_reference(Control *p_control);
    void set_bottom_editor(Control *p_control);

    void set_use_folding(bool p_use_folding) { use_folding = p_use_folding; }
    bool is_using_folding() const { return use_folding; }

    virtual void expand_all_folding();
    virtual void collapse_all_folding();

    Variant get_drag_data(const Point2 &p_point) override;

    void set_selectable(bool p_selectable) { selectable = p_selectable; }
    bool is_selectable() const { return selectable; }

    void set_name_split_ratio(float p_ratio) { split_ratio = p_ratio; }
    float get_name_split_ratio() const { return split_ratio; }

    void set_object_and_property(Object *p_object, const StringName &p_property);
    Control *make_custom_tooltip(StringView p_text) const override;

    void set_draw_top_bg(bool p_draw) { draw_top_bg = p_draw; }

    bool can_revert_to_default() const { return can_revert; }

    EditorProperty();
};

class GODOT_EXPORT EditorInspectorPlugin : public RefCounted {
    GDCLASS(EditorInspectorPlugin,RefCounted)

    friend class EditorInspector;
    struct AddedEditor {
        Control *property_editor;
        Vector<String> properties;
        String label;
    };

    Vector<AddedEditor> added_editors;

protected:
    static void _bind_methods();

public:
    void add_custom_control(Control *control);
    void add_property_editor(StringView p_for_property, Control *p_prop);
    void add_property_editor_for_multiple_properties(
            StringView p_label, const Vector<String> &p_properties, Control *p_prop);

    virtual bool can_handle(Object *p_object);
    virtual void parse_begin(Object *p_object);
    virtual void parse_category(Object *p_object, StringView p_parse_category);
    virtual bool parse_property(Object *p_object, VariantType p_type, StringView p_path, PropertyHint p_hint,
            StringView p_hint_text, int p_usage);
    virtual void parse_end();
};

class GODOT_EXPORT EditorInspectorCategory : public Control {
    GDCLASS(EditorInspectorCategory,Control)

    friend class EditorInspector;
    Ref<Texture> icon;
    String label;
    Color bg_color;

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    Size2 get_minimum_size() const override;
    Control *make_custom_tooltip(StringView p_text) const override;

    EditorInspectorCategory();
};
class VBoxContainer;

class GODOT_EXPORT EditorInspectorSection : public Container {
    GDCLASS(EditorInspectorSection,Container)

    String label;
    String section;
    Color bg_color;
    Object *object;
    VBoxContainer *vbox;
    bool vbox_added; //optimization
    bool foldable;

    void _test_unfold();

protected:
    void _notification(int p_what);
    static void _bind_methods();
    void _gui_input(const Ref<InputEvent> &p_event);

public:
    Size2 get_minimum_size() const override;

    void setup(StringView p_section, StringView p_label, Object *p_object, const Color &p_bg_color, bool p_foldable);
    VBoxContainer *get_vbox();
    void unfold();
    void fold();


    EditorInspectorSection();
    ~EditorInspectorSection() override;
};

class GODOT_EXPORT EditorInspector : public ScrollContainer {
    GDCLASS(EditorInspector,ScrollContainer)

    UndoRedo *undo_redo;
    enum { MAX_PLUGINS = 1024 };
    static FixedVector<Ref<EditorInspectorPlugin>,MAX_PLUGINS> inspector_plugins;

    VBoxContainer *main_vbox;

    //map use to cache the instanced editors
    HashMap<StringName, Vector<EditorProperty *> > editor_property_map;
    Vector<EditorInspectorSection *> sections;
    HashSet<StringName> pending;

    void _clear();
    Object *object = nullptr;

    //

    LineEdit *search_box = nullptr;
    float refresh_countdown;
    StringName _prop_edited;
    StringName property_selected;
    int property_focusable;
    int update_scroll_request;
    int changing;
    bool show_categories = false;
    bool hide_script;
    bool use_doc_hints;
    EditorPropertyNameStyle property_name_style;
    bool use_filter;
    bool autoclear;
    bool use_folding;
    bool update_all_pending;
    bool read_only;
    bool keying;
    bool sub_inspector;

    bool update_tree_pending;

    HashMap<StringName, HashMap<StringName, String> > descr_cache;
    HashMap<StringName, String> class_descr_cache;
    HashSet<StringName> restart_request_props;

    HashMap<GameEntity, int> scroll_cache;

    String property_prefix; //used for sectioned inspector
    StringName object_class;
    Variant property_clipboard;

    void _edit_set(StringView p_name, const Variant &p_value, bool p_refresh_all, StringView p_changed_field);

    void _property_changed(
            StringView p_path, const Variant &p_value, StringView p_name = StringView(), bool changing = false);
    void _property_changed_update_all(
            StringView p_path, const Variant &p_value, StringView p_name = {}, bool p_changing = false);
    void _multiple_properties_changed(const Vector<String> &p_paths, Array p_values);
    void _property_keyed(const StringName &p_path, bool p_advance);
    void _property_keyed_with_value(StringView p_path, const Variant &p_value, bool p_advance);

    void _property_checked(const StringName &p_path, bool p_checked);
    void _property_pinned(const StringName &p_path, bool p_pinned);

    void _resource_selected(StringView p_path, const RES& p_resource);
    void _property_selected(const StringName &p_path, int p_focusable);
    void _object_id_selected(StringView p_path, GameEntity p_id);

    void _node_removed(Node *p_node);

    void _changed_callback(Object *p_changed, StringName p_prop) override;
    void _edit_request_change(Object *p_object, StringView p_prop);

    void _filter_changed(StringView /*p_text*/);
    void _parse_added_editors(VBoxContainer *current_vbox, const Ref<EditorInspectorPlugin>& ped);

    void _vscroll_changed(double);

    void _feature_profile_changed();

    bool _is_property_disabled_by_feature_profile(const StringName &p_property);
    void _update_inspector_bg();

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    static void add_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin);
    static void remove_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin);
    static void cleanup_plugins();

    static EditorProperty *instantiate_property_editor(Object *p_object, VariantType p_type, StringView p_path,
            PropertyHint p_hint, StringView p_hint_text, int p_usage);

    void set_undo_redo(UndoRedo *p_undo_redo);

    const StringName &get_selected_path() const { return property_selected; }

    void update_tree();
    void update_property(const StringName &p_prop);

    void refresh();

    void edit(Object *p_object);
    Object *get_edited_object();

    void set_keying(bool p_active);
    void set_read_only(bool p_read_only);

	EditorPropertyNameStyle get_property_name_style() const;
    void set_property_name_style(EditorPropertyNameStyle p_style);
    void set_autoclear(bool p_enable);

    void set_show_categories(bool p_show);
    void set_use_doc_hints(bool p_enable);
    void set_hide_script(bool p_hide);

    void set_use_filter(bool p_use);
    void register_text_enter(Node *p_line_edit);

    void set_use_folding(bool p_enable);
    bool is_using_folding();

    void collapse_all_folding();
    void expand_all_folding();

    void set_scroll_offset(int p_offset);
    int get_scroll_offset() const;

    void set_property_prefix(const String &p_prefix);
    const String &get_property_prefix() const;

    void set_object_class(const StringName &p_class);
    // const StringName &get_object_class() const;

    void set_sub_inspector(bool p_enable);
    bool is_sub_inspector() const { return sub_inspector; }

    void set_property_clipboard(const Variant &p_value);
    Variant get_property_clipboard() const;

    EditorInspector();
private:
    String process_doc_hints(const PropertyInfo &pi);
};
// internal helper function used by inspector and sectioned_inspector
extern bool _property_path_matches(StringView p_property_path, StringView p_filter, EditorPropertyNameStyle p_style);
