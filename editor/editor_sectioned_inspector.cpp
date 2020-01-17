/*************************************************************************/
/*  editor_sectioned_inspector.cpp                                       */
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

#include "editor_sectioned_inspector.h"
#include "editor_scale.h"
#include "core/object_db.h"
#include "core/object_tooling.h"
#include "core/method_bind.h"

IMPL_GDCLASS(SectionedInspector)

class SectionedInspectorFilter : public Object {

    GDCLASS(SectionedInspectorFilter,Object)

    Object *edited;
    String section;
    bool allow_sub;

    bool _set(const StringName &p_name, const Variant &p_value) {

        if (!edited)
            return false;

        StringName name = p_name;
        if (!section.empty()) {
            name = StringName(section + "/" + name);
        }

        bool valid;
        edited->set(name, p_value, &valid);
        return valid;
    }

    bool _get(const StringName &p_name, Variant &r_ret) const {

        if (!edited)
            return false;

        StringName name = p_name;
        if (!section.empty()) {
            name = StringName(section + "/" + name);
        }

        bool valid = false;

        r_ret = edited->get(name, &valid);
        return valid;
    }
    void _get_property_list(ListPOD<PropertyInfo> *p_list) const {

        if (!edited)
            return;

        ListPOD<PropertyInfo> pinfo;
        edited->get_property_list(&pinfo);
        for (const PropertyInfo &E : pinfo) {

            PropertyInfo pi = E;
            auto sp = StringUtils::find(pi.name,"/");

            if (pi.name == "resource_path" || pi.name == "resource_name" || pi.name == "resource_local_to_scene" || StringUtils::begins_with(pi.name,"script/") || StringUtils::begins_with(pi.name,"_global_script")) //skip resource stuff
                continue;

            if (sp == String::npos) {
                pi.name = StringName(String("global/") + pi.name);
            }

            if (StringUtils::begins_with(pi.name,section + "/")) {
                pi.name = StringName(StringUtils::replace_first(pi.name,section + "/", String()));
                if (!allow_sub && StringUtils::contains(pi.name,"/"))
                    continue;
                p_list->push_back(pi);
            }
        }
    }

    bool property_can_revert(const String &p_name) {

        return edited->call("property_can_revert", section + "/" + p_name);
    }

    Variant property_get_revert(const String &p_name) {

        return edited->call("property_get_revert", section + "/" + p_name);
    }

protected:
    static void _bind_methods() {

        MethodBinder::bind_method("property_can_revert", &SectionedInspectorFilter::property_can_revert);
        MethodBinder::bind_method("property_get_revert", &SectionedInspectorFilter::property_get_revert);
    }

public:
    void set_section(const String &p_section, bool p_allow_sub) {

        section = p_section;
        allow_sub = p_allow_sub;
        Object_change_notify(this);
    }

    void set_edited(Object *p_edited) {
        edited = p_edited;
        Object_change_notify(this);
    }

    SectionedInspectorFilter() {
        edited = nullptr;
    }
};
IMPL_GDCLASS(SectionedInspectorFilter)

void SectionedInspector::_bind_methods() {

    MethodBinder::bind_method("_section_selected", &SectionedInspector::_section_selected);
    MethodBinder::bind_method("_search_changed", &SectionedInspector::_search_changed);

    MethodBinder::bind_method("update_category_list", &SectionedInspector::update_category_list);
}

void SectionedInspector::_section_selected() {

    if (!sections->get_selected())
        return;

    selected_category = sections->get_selected()->get_metadata(0).as<String>();
    filter->set_section(selected_category, sections->get_selected()->get_children() == nullptr);
    inspector->set_property_prefix(selected_category + "/");
}

void SectionedInspector::set_current_section(const String &p_section) {

    if (section_map.contains(p_section)) {
        section_map[p_section]->select(0);
    }
}

String SectionedInspector::get_current_section() const {

    if (sections->get_selected())
        return sections->get_selected()->get_metadata(0);
    else
        return String();
}

String SectionedInspector::get_full_item_path(const String &p_item) {

    String base = get_current_section();

    if (!base.empty())
        return base + "/" + p_item;
    else
        return p_item;
}

void SectionedInspector::edit(Object *p_object) {

    if (!p_object) {
        obj = 0;
        sections->clear();

        filter->set_edited(nullptr);
        inspector->edit(nullptr);

        return;
    }

    ObjectID id = p_object->get_instance_id();

    inspector->set_object_class(StringName(p_object->get_class()));

    if (obj != id) {

        obj = id;
        update_category_list();

        filter->set_edited(p_object);
        inspector->edit(filter);

        TreeItem *first_item = sections->get_root();
        if (first_item) {
            while (first_item->get_children())
                first_item = first_item->get_children();

            first_item->select(0);
            selected_category = first_item->get_metadata(0).as<String>();
        }
    } else {

        update_category_list();
    }
}

void SectionedInspector::update_category_list() {

    sections->clear();

    Object *o = ObjectDB::get_instance(obj);

    if (!o)
        return;

    ListPOD<PropertyInfo> pinfo;
    o->get_property_list(&pinfo);

    section_map.clear();

    TreeItem *root = sections->create_item();
    section_map[String()] = root;

    String filter;
    if (search_box)
        filter = search_box->get_text();

    for (const PropertyInfo &pi : pinfo) {

        if (pi.usage & PROPERTY_USAGE_CATEGORY)
            continue;
        else if (!(pi.usage & PROPERTY_USAGE_EDITOR))
            continue;

        if (StringUtils::contains(pi.name, ':') || pi.name == "script" || pi.name == "resource_name" ||
                pi.name == "resource_path" || pi.name == "resource_local_to_scene" ||
                StringUtils::begins_with(pi.name, "_global_script"))
            continue;

        auto sp = StringUtils::find(pi.name,"/");
        String p_name(pi.name);
        if (sp == String::npos)
            p_name = "global/" + p_name;

        PODVector<se_string_view> sectionarr = StringUtils::split(p_name,'/');
        String metasection;

        if (!filter.empty() && !StringUtils::is_subsequence_of(filter,StringUtils::capitalize(sectionarr[sectionarr.size() - 1]),StringUtils::CaseInsensitive))
            continue;

        int sc = MIN(2, sectionarr.size() - 1);

        for (int i = 0; i < sc; i++) {

            TreeItem *parent = section_map[metasection];
            parent->set_custom_bg_color(0, get_color("prop_subsection", "Editor"));

            if (i > 0) {
                metasection += String("/") + sectionarr[i];
            } else {
                metasection = sectionarr[i];
            }

            if (!section_map.contains(metasection)) {
                TreeItem *ms = sections->create_item(parent);
                section_map[metasection] = ms;
                ms->set_text_utf8(0, StringUtils::capitalize(sectionarr[i]));
                ms->set_metadata(0, metasection);
                ms->set_selectable(0, false);
            }

            if (i == sc - 1) {
                //if it has children, make selectable
                section_map[metasection]->set_selectable(0, true);
            }
        }
    }

    if (section_map.contains(selected_category)) {
        section_map[selected_category]->select(0);
    }

    inspector->update_tree();
}

void SectionedInspector::register_search_box(LineEdit *p_box) {

    search_box = p_box;
    inspector->register_text_enter(p_box);
    search_box->connect("text_changed", this, "_search_changed");
}

void SectionedInspector::_search_changed(const String &p_what) {

    update_category_list();
}

EditorInspector *SectionedInspector::get_inspector() {

    return inspector;
}

SectionedInspector::SectionedInspector() :
        obj(0),
        sections(memnew(Tree)),
        filter(memnew(SectionedInspectorFilter)),
        inspector(memnew(EditorInspector)),
        search_box(nullptr) {
    add_constant_override("autohide", 1); // Fixes the dragger always showing up

    VBoxContainer *left_vb = memnew(VBoxContainer);
    left_vb->set_custom_minimum_size(Size2(190, 0) * EDSCALE);
    add_child(left_vb);

    sections->set_v_size_flags(SIZE_EXPAND_FILL);
    sections->set_hide_root(true);

    left_vb->add_child(sections, true);

    VBoxContainer *right_vb = memnew(VBoxContainer);
    right_vb->set_custom_minimum_size(Size2(300, 0) * EDSCALE);
    right_vb->set_h_size_flags(SIZE_EXPAND_FILL);
    add_child(right_vb);

    inspector->set_v_size_flags(SIZE_EXPAND_FILL);
    right_vb->add_child(inspector, true);
    inspector->set_use_doc_hints(true);

    sections->connect("cell_selected", this, "_section_selected");
}

SectionedInspector::~SectionedInspector() {

    memdelete(filter);
}

void register_sectioned_inspector_classes()
{
    SectionedInspector::initialize_class();
    SectionedInspectorFilter::initialize_class();

}
