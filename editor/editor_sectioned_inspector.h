/*************************************************************************/
/*  editor_sectioned_inspector.h                                         */
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
#include "scene/gui/split_container.h"
#include "scene/gui/tree.h"

class SectionedInspectorFilter;

class SectionedInspector : public HSplitContainer {

    GDCLASS(SectionedInspector,HSplitContainer)

    HashMap<String, TreeItem *> section_map;
    String selected_category;

    Tree *sections;
    SectionedInspectorFilter *filter;

    EditorInspector *inspector;
    LineEdit *search_box = nullptr;

    GameEntity obj;

    static void _bind_methods();
    void _section_selected();

    void _search_changed(const String &p_what);
protected:
    void _notification(int p_what);

public:
    void register_search_box(LineEdit *p_box);
    EditorInspector *get_inspector();
    void edit(Object *p_object);
    String get_full_item_path(const String &p_item);

    void set_current_section(const String &p_section);
    String get_current_section() const;

    void update_category_list();

    SectionedInspector();
    ~SectionedInspector() override;
};

void register_sectioned_inspector_classes();
