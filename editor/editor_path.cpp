/*************************************************************************/
/*  editor_path.cpp                                                      */
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

#include "editor_path.h"

#include "editor_node.h"
#include "editor_scale.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/object_db.h"

IMPL_GDCLASS(EditorPath)

void EditorPath::_add_children_to_popup(Object *p_obj, int p_depth) {

    if (p_depth > 8)
        return;

    Vector<PropertyInfo> pinfo;
    p_obj->get_property_list(&pinfo);
    for (PropertyInfo &E : pinfo) {

        if (!(E.usage & PROPERTY_USAGE_EDITOR))
            continue;
        if (E.hint != PropertyHint::ResourceType)
            continue;

        Variant value = p_obj->get(E.name);
        if (value.get_type() != VariantType::OBJECT)
            continue;
        Object *obj = value.as<Object *>();
        if (!obj)
            continue;

        Ref<Texture> icon = EditorNode::get_singleton()->get_object_icon(obj);

        int index = get_popup()->get_item_count();
        get_popup()->add_icon_item(icon, StringName(StringUtils::capitalize(E.name)), objects.size());
        get_popup()->set_item_h_offset(index, p_depth * 10 * EDSCALE);
        objects.push_back(obj->get_instance_id());

        _add_children_to_popup(obj, p_depth + 1);
    }
}

void EditorPath::_about_to_show() {

    Object *obj = gObjectDB().get_instance(history->get_path_object(history->get_path_size() - 1));
    if (!obj)
        return;

    objects.clear();
    get_popup()->clear();
    get_popup()->set_size(Size2(get_size().width, 1));
    _add_children_to_popup(obj);
    if (get_popup()->get_item_count() == 0) {
        get_popup()->add_item(TTR("No sub-resources found."));
        get_popup()->set_item_disabled(0, true);
    }
}

void EditorPath::update_path() {

    for (int i = 0; i < history->get_path_size(); i++) {

        Object *obj = gObjectDB().get_instance(history->get_path_object(i));
        if (!obj)
            continue;

        Ref<Texture> icon = EditorNode::get_singleton()->get_object_icon(obj);
        if (icon)
            set_button_icon(icon);

        if (i != history->get_path_size() - 1)
            continue;

        String name;
        if (object_cast<Resource>(obj)) {

            Resource *r = object_cast<Resource>(obj);
            if (PathUtils::is_resource_file(r->get_path()))
                name = PathUtils::get_file(r->get_path());
            else
                name = r->get_name();

            if (name.empty())
                name = r->get_class();
        } else if (obj->is_class("ScriptEditorDebuggerInspectedObject"))
            name = obj->call_va("get_title").as<String>();
        else if (object_cast<Node>(obj))
            name = object_cast<Node>(obj)->get_name();
        else if (object_cast<Resource>(obj) && !object_cast<Resource>(obj)->get_name().empty())
            name = object_cast<Resource>(obj)->get_name();
        else
            name = obj->get_class();

        set_text_utf8(" " + name); // An extra space so the text is not too close of the icon.
        set_tooltip_utf8(obj->get_class());
    }
}

void EditorPath::_id_pressed(int p_idx) {

    ERR_FAIL_INDEX(p_idx, objects.size());

    Object *obj = gObjectDB().get_instance(objects[p_idx]);
    if (!obj)
        return;

    EditorNode::get_singleton()->push_item(obj);
}
void EditorPath::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_THEME_CHANGED: {
            update_path();
        } break;
    }
}
void EditorPath::_bind_methods() {

    MethodBinder::bind_method("_about_to_show", &EditorPath::_about_to_show);
    MethodBinder::bind_method("_id_pressed", &EditorPath::_id_pressed);
}

EditorPath::EditorPath(EditorHistory *p_history) {

    history = p_history;
    set_clip_text(true);
    set_text_align(ALIGN_LEFT);
    get_popup()->connect("about_to_show",callable_mp(this, &ClassName::_about_to_show));
    get_popup()->connect("id_pressed",callable_mp(this, &ClassName::_id_pressed));
}
