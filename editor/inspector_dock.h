/*************************************************************************/
/*  inspector_dock.h                                                     */
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

#include "scene/gui/box_container.h"

class EditorNode;
class ToolButton;
class CreateDialog;
class MenuButton;
class LineEdit;
class EditorPath;
class Button;
class AcceptDialog;
enum class EditorPropertyNameStyle : uint8_t;

using RES = Ref<Resource>;

class InspectorDock : public VBoxContainer {

    GDCLASS(InspectorDock,VBoxContainer)
    friend class SceneTreeDock;
    enum MenuOptions {
        RESOURCE_LOAD,
        RESOURCE_SAVE,
        RESOURCE_SAVE_AS,
        RESOURCE_MAKE_BUILT_IN,
        RESOURCE_COPY,
        RESOURCE_EDIT_CLIPBOARD,
        OBJECT_COPY_PARAMS,
        OBJECT_PASTE_PARAMS,
        OBJECT_UNIQUE_RESOURCES,
        OBJECT_REQUEST_HELP,

        COLLAPSE_ALL,
        EXPAND_ALL,

        // Matches `EditorPropertyNameStyle`.
        PROPERTY_NAME_STYLE_RAW,
        PROPERTY_NAME_STYLE_CAPITALIZED,
        PROPERTY_NAME_STYLE_LOCALIZED,
        OBJECT_METHOD_BASE = 500
    };

    EditorNode *editor;
    class EditorData *editor_data;

    class EditorInspector *inspector;

    Object *current;

    ToolButton *backward_button;
    ToolButton *forward_button;

    class EditorFileDialog *load_resource_dialog;
    CreateDialog *new_resource_dialog;
    ToolButton *resource_new_button;
    ToolButton *resource_load_button;
    MenuButton *resource_save_button;
    MenuButton *resource_extra_button;
    MenuButton *history_menu;
    LineEdit *search;

    Button *open_docs_button;
    MenuButton *object_menu;
    EditorPath *editor_path;

    Button *warning;
    AcceptDialog *warning_dialog;

    EditorPropertyNameStyle property_name_style;

    void _prepare_menu();
    void _menu_option(int p_option);

    void _new_resource();
    void _load_resource(StringView p_type = StringName());
    void _open_resource_selector() { _load_resource(); } // just used to call from arg-less signal
    void _resource_file_selected(StringView p_file);
    void _save_resource(bool save_as) const;
    void _unref_resource() const;
    void _copy_resource() const;
    void _paste_resource() const;
    void _prepare_resource_extra_popup();

    void _warning_pressed();
    void _resource_created();
    void _resource_selected(const RES &p_res, const StringName &p_property = StringName());
    void _edit_forward();
    void _edit_back();
    void _menu_collapseall();
    void _menu_expandall();
    void _select_history(int p_idx);
private:
    void _prepare_history();
protected:

    void _property_keyed(StringView p_keyed, const Variant &p_value, bool p_advance);
public: // slots
    void _transform_keyed(Object *sp, StringView p_sub, const Transform &p_key);

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    void go_back();
    void update_keying();
    void edit_resource(const Ref<Resource> &p_resource);
    void open_resource(StringView p_type);
    void clear();
    void set_warning(const StringName &p_message);
    void update(Object *p_object);
    Container *get_addon_area();
    EditorInspector *get_inspector() { return inspector; }
    EditorPropertyNameStyle get_property_name_style() const;

    InspectorDock(EditorNode *p_editor, EditorData &p_editor_data);
    ~InspectorDock() override;
};

