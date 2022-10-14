/*************************************************************************/
/*  editor_properties.h                                                  */
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


#include "editor/editor_inspector.h"
#include "scene/gui/line_edit.h"

class EditorSpinSlider;
class PropertySelector;
class ColorPickerButton;
class EditorFileDialog;
class TextEdit;
class AcceptDialog;
class Button;
class OptionButton;
class CheckBox;
class SceneTreeDialog;
class TextureRect;
class EditorResourcePicker;

class EditorPropertyNil : public EditorProperty {
    GDCLASS(EditorPropertyNil,EditorProperty)
    LineEdit *text;

public:
    void update_property() override;
    EditorPropertyNil();
};

class EditorPropertyText : public EditorProperty {
    GDCLASS(EditorPropertyText,EditorProperty)

    LineEdit *text;

    bool updating;
    bool string_name;
    void _text_changed(StringView p_string);
    void _text_entered(StringView p_string);

protected:
    static void _bind_methods();

public:
    void set_string_name(bool p_enabled);
    void update_property() override;
    void set_placeholder(const StringName &p_string);
    EditorPropertyText();
};

class EditorPropertyMultilineText : public EditorProperty {
    GDCLASS(EditorPropertyMultilineText,EditorProperty)

    TextEdit *text;

    AcceptDialog *big_text_dialog;
    TextEdit *big_text;
    Button *open_big_text;

    void _big_text_changed();
    void _text_changed();
    void _open_big_text();

protected:
    void _notification(int p_what);
    static void _bind_methods() {}

public:
    void update_property() override;
    EditorPropertyMultilineText();
};
class HBoxContainer;

class EditorPropertyTextEnum : public EditorProperty {
    GDCLASS(EditorPropertyTextEnum,EditorProperty)

    HBoxContainer *default_layout;
    HBoxContainer *edit_custom_layout;

    OptionButton *option_button;
    Button *edit_button;

    LineEdit *custom_value_edit;
    Button *accept_button;
    Button *cancel_button;

    Vector<StringName> options;
    bool loose_mode = false;
    bool string_name;
    void _emit_changed_value(StringView p_string);
    void _option_selected(int p_which);

    void _edit_custom_value();
    void _custom_value_submitted(String p_value);
    void _custom_value_accepted();
    void _custom_value_cancelled();

protected:
    static void _bind_methods() { }
    void _notification(int p_what);

public:
    void setup(const Vector<StringView> &p_options, bool p_string_name = false, bool p_loose_mode = false);
    void update_property() override;
    EditorPropertyTextEnum();
};

class EditorPropertyPath : public EditorProperty {
    GDCLASS(EditorPropertyPath,EditorProperty)

    Vector<String> extensions;
    bool folder;
    bool global;
    bool save_mode;
    EditorFileDialog *dialog;
    LineEdit *path;
    Button *path_edit;

    void _path_selected(StringView p_path);
    void _path_pressed();
    void _path_focus_exited();

protected:
    static void _bind_methods() { }
    void _notification(int p_what);

public:
    void setup(const Vector<StringView> &p_extensions, bool p_folder, bool p_global);
    void set_save_mode();
    void update_property() override;
    EditorPropertyPath();
};

class EditorPropertyClassName : public EditorProperty {
    GDCLASS(EditorPropertyClassName,EditorProperty)

private:
    class CreateDialog *dialog;
    Button *property;
    StringName selected_type;
    StringName base_type;
    void _property_selected();
    void _dialog_created();

protected:
    static void _bind_methods() {}

public:
    void setup(const StringName &p_base_type, const StringName &p_selected_type);
    void update_property() override;
    EditorPropertyClassName();
};

class EditorPropertyMember : public EditorProperty {
    GDCLASS(EditorPropertyMember,EditorProperty)

public:
    enum Type {
        MEMBER_PROPERTY_OF_VARIANT_TYPE, ///< a property of a type

    };

private:
    Type hint;
    PropertySelector *selector;
    Button *property;
    String hint_text;

    void _property_selected(StringView p_selected);
    void _property_select();

protected:
    static void _bind_methods() {}

public:
    void setup(Type p_hint, StringView p_hint_text);
    void update_property() override;
    EditorPropertyMember();
};

class EditorPropertyCheck : public EditorProperty {
    GDCLASS(EditorPropertyCheck,EditorProperty)

    CheckBox *checkbox;

    void _checkbox_pressed();

protected:
    static void _bind_methods() {}

public:
    void update_property() override;
    EditorPropertyCheck();
};

class EditorPropertyEnum : public EditorProperty {
    GDCLASS(EditorPropertyEnum,EditorProperty)

    OptionButton *options;

    void _option_selected(int p_which);

protected:
    static void _bind_methods() {}

public:
    void setup(const Vector<StringView> &p_options);
    void update_property() override;
    void set_option_button_clip(bool p_enable);
    EditorPropertyEnum();
};

class EditorPropertyFlags : public EditorProperty {
    GDCLASS(EditorPropertyFlags,EditorProperty)

    VBoxContainer *vbox;
    Vector<CheckBox *> flags;
    Vector<int> flag_indices;

    void _flag_toggled();

protected:
    static void _bind_methods() {}

public:
    void setup(const Vector<StringView> &p_options);
    void update_property() override;
    EditorPropertyFlags();
};

class EditorPropertyLayersGrid;

class EditorPropertyLayers : public EditorProperty {
    GDCLASS(EditorPropertyLayers,EditorProperty)

public:
    enum LayerType {
        LAYER_PHYSICS_2D,
        LAYER_RENDER_2D,
        LAYER_PHYSICS_3D,
        LAYER_RENDER_3D,
    };

private:
    EditorPropertyLayersGrid *grid;
    void _grid_changed(uint32_t p_grid);
    LayerType layer_type;
    PopupMenu *layers;
    Button *button;

    void _button_pressed();
    void _menu_pressed(int p_menu);
    void _refresh_names();

protected:
    static void _bind_methods() { }

public:
    void setup(LayerType p_layer_type);
    void update_property() override;
    EditorPropertyLayers();
};

class EditorPropertyInteger : public EditorProperty {
    GDCLASS(EditorPropertyInteger,EditorProperty)

    EditorSpinSlider *spin;
    bool setting;
    void _value_changed(int64_t p_val);

protected:
    static void _bind_methods() { }

public:
    void update_property() override;
    void setup(int64_t p_min, int64_t p_max, int64_t p_step, bool p_allow_greater, bool p_allow_lesser);
    EditorPropertyInteger();
};

class EditorPropertyObjectID : public EditorProperty {
    GDCLASS(EditorPropertyObjectID,EditorProperty)

    Button *edit;
    StringName base_type;
    void _edit_pressed();

protected:
    static void _bind_methods() {}

public:
    void update_property() override;
    void setup(const StringName &p_base_type);
    EditorPropertyObjectID();
};

class EditorPropertyFloat : public EditorProperty {
    GDCLASS(EditorPropertyFloat,EditorProperty)

    EditorSpinSlider *spin;
    bool setting;
    void _value_changed(double p_val);

protected:
    static void _bind_methods() {}

public:
    void update_property() override;
    void setup(double p_min, double p_max, double p_step, bool p_no_slider, bool p_exp_range, bool p_greater, bool p_lesser);
    EditorPropertyFloat();
};

class EditorPropertyEasing : public EditorProperty {
    GDCLASS(EditorPropertyEasing,EditorProperty)

    Control *easing_draw;
    PopupMenu *preset;
    EditorSpinSlider *spin;
    bool setting;

    bool dragging;
    bool full;
    bool flip;

    enum {
        EASING_ZERO,
        EASING_LINEAR,
        EASING_IN,
        EASING_OUT,
        EASING_IN_OUT,
        EASING_OUT_IN,
        EASING_MAX

    };

    void _drag_easing(const Ref<InputEvent> &p_ev);
    void _draw_easing();
    void _set_preset(int);

    void _setup_spin();
    void _spin_value_changed(double p_value);
    void _spin_focus_exited();

    void _notification(int p_what);

protected:
    static void _bind_methods() { }

public:
    void update_property() override;
    void setup(bool p_full, bool p_flip);
    EditorPropertyEasing();
};

class EditorPropertyVector2 : public EditorProperty {
    GDCLASS(EditorPropertyVector2,EditorProperty)

    EditorSpinSlider *spin[2];
    bool setting;
    void _value_changed(double p_val, const StringName &p_name);

protected:
    void _notification(int p_what);
    static void _bind_methods() {}

public:
    void update_property() override;
    void setup(double p_min, double p_max, double p_step, bool p_no_slider);
    EditorPropertyVector2();
};

class EditorPropertyRect2 : public EditorProperty {
    GDCLASS(EditorPropertyRect2,EditorProperty)

    EditorSpinSlider *spin[4];
    bool setting;
    void _value_changed(double p_val, const StringName &p_name);

protected:
    void _notification(int p_what);
    static void _bind_methods() {}

public:
    void update_property() override;
    void setup(double p_min, double p_max, double p_step, bool p_no_slider);
    EditorPropertyRect2();
};

class EditorPropertyVector3 : public EditorProperty {
    GDCLASS(EditorPropertyVector3,EditorProperty)

    EditorSpinSlider *spin[3];
    bool setting;
    void _value_changed(double p_val, const StringName &p_name);

protected:
    void _notification(int p_what);
    static void _bind_methods() {}

public:
    void update_property() override;
    void setup(double p_min, double p_max, double p_step, bool p_no_slider);
    EditorPropertyVector3();
};

class EditorPropertyPlane : public EditorProperty {
    GDCLASS(EditorPropertyPlane,EditorProperty)

    EditorSpinSlider *spin[4];
    bool setting;
    void _value_changed(double p_val, const StringName &p_name);

protected:
    void _notification(int p_what);
    static void _bind_methods() {}

public:
    void update_property() override;
    void setup(double p_min, double p_max, double p_step, bool p_no_slider);
    EditorPropertyPlane();
};

class EditorPropertyQuat : public EditorProperty {
    GDCLASS(EditorPropertyQuat,EditorProperty)

    EditorSpinSlider *spin[4];
    bool setting;
    void _value_changed(double p_val, const StringName &p_name);

protected:
    void _notification(int p_what);
    static void _bind_methods() {}

public:
    void update_property() override;
    void setup(double p_min, double p_max, double p_step, bool p_no_slider);
    EditorPropertyQuat();
};

class EditorPropertyAABB : public EditorProperty {
    GDCLASS(EditorPropertyAABB,EditorProperty)

    EditorSpinSlider *spin[6];
    bool setting;
    void _value_changed(double p_val, const StringName &p_name);

protected:
    void _notification(int p_what);
    static void _bind_methods() {}

public:
    void update_property() override;
    void setup(double p_min, double p_max, double p_step, bool p_no_slider);
    EditorPropertyAABB();
};

class EditorPropertyTransform2D : public EditorProperty {
    GDCLASS(EditorPropertyTransform2D,EditorProperty)

    EditorSpinSlider *spin[6];
    bool setting;
    void _value_changed(double p_val, const StringName &p_name);

protected:
    void _notification(int p_what);
    static void _bind_methods() {}

public:
    void update_property() override;
    void setup(double p_min, double p_max, double p_step, bool p_no_slider);
    EditorPropertyTransform2D();
};

class EditorPropertyBasis : public EditorProperty {
    GDCLASS(EditorPropertyBasis,EditorProperty)

    EditorSpinSlider *spin[9];
    bool setting;
    void _value_changed(double p_val, const StringName &p_name);

protected:
    void _notification(int p_what);
    static void _bind_methods() {}

public:
    void update_property() override;
    void setup(double p_min, double p_max, double p_step, bool p_no_slider);
    EditorPropertyBasis();
};

class EditorPropertyTransform : public EditorProperty {
    GDCLASS(EditorPropertyTransform,EditorProperty)

    EditorSpinSlider *spin[12];
    bool setting;
    void _value_changed(double p_val, const StringName &p_name);

protected:
    void _notification(int p_what);
    static void _bind_methods() {}

public:
    void update_property() override;
    void setup(double p_min, double p_max, double p_step, bool p_no_slider);
    EditorPropertyTransform();
};

class EditorPropertyColor : public EditorProperty {
    GDCLASS(EditorPropertyColor,EditorProperty)

    ColorPickerButton *picker;
    void _color_changed(const Color &p_color);
    void _popup_closed();

    void _picker_created();
    void _picker_opening();

    Color last_color;

protected:
    static void _bind_methods() { }

public:
    void update_property() override;
    void setup(bool p_show_alpha);
    EditorPropertyColor();
};

class EditorPropertyNodePath : public EditorProperty {
    GDCLASS(EditorPropertyNodePath,EditorProperty)

    Button *assign;
    Button *clear;
    SceneTreeDialog *scene_tree;
    NodePath base_hint;
    bool use_path_from_scene_root;

    Vector<StringName> valid_types;
    void _node_selected(const NodePath &p_path);
    void _node_assign();
    void _node_clear();

    bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
    void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);
    bool is_drop_valid(const Dictionary &p_drag_data) const;
protected:
    static void _bind_methods() { }
    void _notification(int p_what);

public:
    void update_property() override;
    void setup(const NodePath &p_base_hint, Vector<StringName> &&p_valid_types, bool p_use_path_from_scene_root = true);
    EditorPropertyNodePath();
};

class EditorPropertyRID : public EditorProperty {
    GDCLASS(EditorPropertyRID,EditorProperty)

    Label *label;

public:
    void update_property() override;
    EditorPropertyRID();
};

class EditorPropertyResource : public EditorProperty {
    GDCLASS(EditorPropertyResource,EditorProperty)

    EditorResourcePicker *resource_picker = nullptr;
    SceneTreeDialog *scene_tree = nullptr;

    bool use_sub_inspector = false;
    EditorInspector *sub_inspector = nullptr;
    VBoxContainer *sub_inspector_vbox = nullptr;
    bool updating_theme = false;
    bool opened_editor = false;

    void _resource_selected(const RES &p_resource, bool p_edit);
    void _resource_changed(const RES &p_resource);
    void _viewport_selected(const NodePath &p_path);


    void _sub_inspector_property_keyed(StringView p_property, const Variant &p_value, bool);
    void _sub_inspector_resource_selected(const RES &p_resource, StringView p_property);
    void _sub_inspector_object_id_selected(int p_id);

    bool _can_use_sub_inspector(const RES &p_resource);
    void _open_editor_pressed();
    void _fold_other_editors(Object *p_self);

    void _update_property_bg();

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    void update_property() override;
    void setup(Object *p_object, StringView p_path, const StringName &p_base_type);

    void collapse_all_folding() override;
    void expand_all_folding() override;

    void set_use_sub_inspector(bool p_enable);

    EditorPropertyResource();
};

///////////////////////////////////////////////////
/// \brief The EditorInspectorDefaultPlugin class
///
class EditorInspectorDefaultPlugin : public EditorInspectorPlugin {
    GDCLASS(EditorInspectorDefaultPlugin,EditorInspectorPlugin)

public:
    bool can_handle(Object *p_object) override;
    void parse_begin(Object *p_object) override;
    bool parse_property(Object *p_object, VariantType p_type, StringView p_path, PropertyHint p_hint, StringView p_hint_text, int p_usage) override;
    void parse_end() override;
};

void register_editor_properties_classes();

