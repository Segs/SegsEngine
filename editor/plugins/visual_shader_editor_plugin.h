/*************************************************************************/
/*  visual_shader_editor_plugin.h                                        */
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

#include "editor/editor_plugin.h"
#include "editor/property_editor.h"
#include "scene/gui/button.h"
#include "scene/gui/graph_edit.h"
#include "scene/gui/popup.h"
#include "scene/gui/tree.h"
#include "scene/resources/visual_shader.h"

// Interface common to all visual shader editor instances returned by create_editor?
struct IVisualShaderEditor {
    virtual void _show_prop_names(bool p_show); // default implementation only tries to call script implementation
};

class VisualShaderNodePlugin : public RefCounted {

    GDCLASS(VisualShaderNodePlugin,RefCounted)

protected:
    static void _bind_methods();

public:
    virtual Control *create_editor(const Ref<Resource> &p_parent_resource, const Ref<VisualShaderNode> &p_node);
};

class VisualShaderEditor : public VBoxContainer {

    GDCLASS(VisualShaderEditor,VBoxContainer)

    CustomPropertyEditor *property_editor;
    int editing_node;
    int editing_port;

    Ref<VisualShader> visual_shader;
    HSplitContainer *main_box;
    GraphEdit *graph;
    ToolButton *add_node;
    ToolButton *preview_shader;

    OptionButton *edit_type;

    PanelContainer *error_panel;
    Label *error_label;

    bool pending_update_preview;
    bool shader_error;
    VBoxContainer *preview_vbox;
    TextEdit *preview_text;
    Label *error_text;

    UndoRedo *undo_redo;
    Point2 saved_node_pos;
    bool saved_node_pos_dirty;

    ConfirmationDialog *members_dialog;
    MenuButton *tools;

    bool preview_showed;

    enum ToolsMenuOptions {
        EXPAND_ALL,
        COLLAPSE_ALL
    };

    Tree *members;
    AcceptDialog *alert;
    LineEdit *node_filter;
    RichTextLabel *node_desc;

    void _tools_menu_option(int p_idx);
    void _show_members_dialog(bool at_mouse_pos);
public: // slots:
    void _update_graph();
private:
    struct AddOption {
        StringName name;
        StringName category;
        StringName sub_category;
        StringName type;
        StringName description;
        int sub_func;
        StringName sub_func_str;
        Ref<Script> script;
        int mode;
        int return_type;
        int func;
        float value;
        bool highend;
        bool is_custom;

        AddOption(const StringName &p_name = StringName(), const StringName &p_category = StringName(), const StringName &p_sub_category = StringName(), const StringName &p_type = StringName(), const StringName &p_description = StringName(), int p_sub_func = -1, int p_return_type = -1, int p_mode = -1, int p_func = -1, float p_value = -1, bool p_highend = false) {
            name = p_name;
            type = p_type;
            category = p_category;
            sub_category = p_sub_category;
            description = p_description;
            sub_func = p_sub_func;
            return_type = p_return_type;
            mode = p_mode;
            func = p_func;
            value = p_value;
            highend = p_highend;
            is_custom = false;
        }
        AddOption(const StringName &p_name, const StringName &p_category, const StringName &p_sub_category, const StringName &p_type, const StringName &p_description, const StringName &p_sub_func, int p_return_type = -1, int p_mode = -1, int p_func = -1, float p_value = -1, bool p_highend = false) {
            name = p_name;
            type = p_type;
            category = p_category;
            sub_category = p_sub_category;
            description = p_description;
            sub_func = 0;
            sub_func_str = p_sub_func;
            return_type = p_return_type;
            mode = p_mode;
            func = p_func;
            value = p_value;
            highend = p_highend;
            is_custom = false;
        }
        AddOption(AddOption &&) = default;
        AddOption& operator=(AddOption &&) = default;
    };

    Vector<AddOption> add_options;
    int texture_node_option_idx;
    int custom_node_option_idx;
    Vector<StringView> keyword_list;
    struct DragOp {
        VisualShader::Type type;
        int node;
        Vector2 from;
        Vector2 to;
    };
    Vector<DragOp> drag_buffer;
    bool drag_dirty = false;
    bool updating;
    int to_node;
    int to_slot;
    int from_node;
    int from_slot;

    Vector2 selection_center;
    int copy_type; // shader type
    Vector<int> copy_nodes_buffer;
    HashSet<int> copy_nodes_excluded_buffer;
    Vector<Ref<VisualShaderNodePlugin> > plugins;

    void _draw_color_over_button(Object *obj, Color p_color);

    void _add_custom_node(StringView p_path);
    void _add_texture_node(StringView p_path);
    VisualShaderNode *_add_node(int p_idx, int p_op_idx = -1);
    void _update_options_menu();

    void _show_preview_text();
    void _update_preview();
    StringName _get_description(int p_idx);



    static VisualShaderEditor *singleton;

    void _node_dragged(const Vector2 &p_from, const Vector2 &p_to, int p_node);
    void _nodes_dragged();

    void _connection_request(const StringName &p_from, int p_from_index, const StringName &p_to, int p_to_index);
    void _disconnection_request(const StringName &p_from, int p_from_index, const StringName &p_to, int p_to_index);

    void _scroll_changed(const Vector2 &p_scroll);
    void _node_selected(Object *p_node);

    void _delete_request(int);
    void _on_nodes_delete();

    void _node_changed(int p_id);

    void _edit_port_default_input(Object *p_button, int p_node, int p_port);
    void _port_edited();


    void _connection_to_empty(const StringName &p_from, int p_from_slot, const Vector2 &p_release_position);
    void _connection_from_empty(const StringName &p_to, int p_to_slot, const Vector2 &p_release_position);

    void _line_edit_changed(StringView p_text, Object *line_edit, int p_node_id);
    void _line_edit_focus_out(Object *line_edit, int p_node_id);

    void _port_name_focus_out(Object *line_edit, int p_node_id, int p_port_id, bool p_output);

    void _dup_copy_nodes(int p_type, Vector<int> &r_nodes, HashSet<int> &r_excluded);
    void _dup_update_excluded(int p_type, HashSet<int> &r_excluded);
    void _dup_paste_nodes(int p_type, int p_pasted_type, Vector<int> &r_nodes, HashSet<int> &r_excluded, const Vector2 &p_offset, bool p_select);

    void _duplicate_nodes();

    void _clear_buffer();
    void _copy_nodes();
    void _paste_nodes();


    void _mode_selected(int p_id);
    void _rebuild();

public:
    // made public for call_deferred lambda
    void _input_select_item(const Ref<VisualShaderNodeInput>& input, const StringName &name);
    void _uniform_select_item(const Ref<VisualShaderNodeUniformRef>& p_uniform_ref, const StringName & p_name);
private:
    void _add_input_port(int p_node, int p_port, int p_port_type, StringView p_name);
    void _remove_input_port(int p_node, int p_port);
    void _change_input_port_type(int p_type, int p_node, int p_port);
    void _change_input_port_name(StringView p_text, Object *line_edit, int p_node, int p_port);

    void _add_output_port(int p_node, int p_port, int p_port_type, StringView p_name);
    void _remove_output_port(int p_node, int p_port);
    void _change_output_port_type(int p_type, int p_node, int p_port);
    void _change_output_port_name(StringView p_text, Object *line_edit, int p_node, int p_port);

    void _expression_focus_out(Object *text_edit, int p_node);

    void _set_node_size(int p_type, int p_node, const Size2 &p_size);
    void _node_resized(const Vector2 &p_new_size, int p_type, int p_node);

    void _preview_select_port(int p_node, int p_port);
    void _graph_gui_input(const Ref<InputEvent> &p_event);

    void _member_filter_changed(StringView p_text);
    void _sbox_input(const Ref<InputEvent> &p_ie);
    void _member_selected();
    void _member_unselected();
    void _member_create();
    void _member_cancel();

    Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
    bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
    void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

    bool _is_available(int p_mode);
    void _update_created_node(GraphNode *node);

protected:
    void _notification(int p_what);
    static void _bind_methods();

    void add_functions();

    void add_vector_ops();

    void add_color_op_nodes();

    void add_spatial_input_ops();

public:
    void update_custom_nodes();
    void add_plugin(const Ref<VisualShaderNodePlugin> &p_plugin);
    void remove_plugin(const Ref<VisualShaderNodePlugin> &p_plugin);

    static VisualShaderEditor *get_singleton() { return singleton; }

    void clear_custom_types();
    void add_custom_type(const StringName &p_name, const Ref<Script> &p_script, const StringName &p_description, int p_return_icon_type, const StringName &p_category, const StringName &p_sub_category);

    Size2 get_minimum_size() const override;
    void edit(VisualShader *p_visual_shader);
    VisualShaderEditor();
};

class VisualShaderEditorPlugin : public EditorPlugin {

    GDCLASS(VisualShaderEditorPlugin,EditorPlugin)

    VisualShaderEditor *visual_shader_editor;
    EditorNode *editor;
    Button *button;

public:
    StringView get_name() const override { return "VisualShader"; }
    bool has_main_screen() const override { return false; }
    void edit(Object *p_object) override;
    bool handles(Object *p_object) const override;
    void make_visible(bool p_visible) override;

    VisualShaderEditorPlugin(EditorNode *p_node);
    ~VisualShaderEditorPlugin() override;
};

class VisualShaderNodePluginDefault : public VisualShaderNodePlugin {

    GDCLASS(VisualShaderNodePluginDefault,VisualShaderNodePlugin)

public:
    Control *create_editor(const Ref<Resource> &p_parent_resource, const Ref<VisualShaderNode> &p_node) override;
};

class EditorPropertyShaderMode : public EditorProperty {
    GDCLASS(EditorPropertyShaderMode,EditorProperty)
    OptionButton *options;

    void _option_selected(int p_which);

protected:
    static void _bind_methods() {}

public:
    void setup(const Vector<StringView> &p_options);
    void update_property() override;
    void set_option_button_clip(bool p_enable);
    EditorPropertyShaderMode();
};

class EditorInspectorShaderModePlugin : public EditorInspectorPlugin {
    GDCLASS(EditorInspectorShaderModePlugin,EditorInspectorPlugin)

public:
    bool can_handle(Object *p_object) override;
    void parse_begin(Object *p_object) override;
    bool parse_property(Object *p_object, VariantType p_type, StringView p_path, PropertyHint p_hint, StringView p_hint_text, int p_usage) override;
    void parse_end() override;
};

class VisualShaderNodePortPreview : public Control {
    GDCLASS(VisualShaderNodePortPreview,Control)
    Ref<VisualShader> shader;
    VisualShader::Type type;
    int node;
    int port;
    void _shader_changed(); //must regen
protected:
    void _notification(int p_what);
    static void _bind_methods() {}

public:
    Size2 get_minimum_size() const override;
    void setup(const Ref<VisualShader> &p_shader, VisualShader::Type p_type, int p_node, int p_port);
    VisualShaderNodePortPreview();
};

class VisualShaderConversionPlugin : public EditorResourceConversionPlugin {
    GDCLASS(VisualShaderConversionPlugin,EditorResourceConversionPlugin)

public:
    StringName converts_to() const override;
    bool handles(const Ref<Resource> &p_resource) const override;
    Ref<Resource> convert(const Ref<Resource> &p_resource) const override;
};

void register_visual_shader_editor_classes();
