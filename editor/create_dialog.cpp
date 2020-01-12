/*************************************************************************/
/*  create_dialog.cpp                                                    */
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

#include "create_dialog.h"
#include "editor_feature_profile.h"

#include "core/class_db.h"
#include "core/method_bind.h"
#include "core/os/keyboard.h"
#include "core/string_formatter.h"
#include "core/translation_helpers.h"

#include "editor_help.h"
#include "editor_node.h"
#include "editor_scale.h"
#include "editor_settings.h"
#include "scene/gui/box_container.h"
#include "EASTL/sort.h"

IMPL_GDCLASS(CreateDialog)

void CreateDialog::popup_create(bool p_dont_clear, bool p_replace_mode, const StringName &p_select_type) {

    type_list.clear();
    ClassDB::get_class_list(&type_list);
    ScriptServer::get_global_class_list(&type_list);
    eastl::sort(type_list.begin(),type_list.end(),WrapAlphaCompare());

    recent->clear();

    FileAccess *f = FileAccess::open(PathUtils::plus_file(EditorSettings::get_singleton()->get_project_settings_dir(),
                                             se_string("create_recent.") + base_type),
            FileAccess::READ);

    if (f) {

        TreeItem *root = recent->create_item();

        while (!f->eof_reached()) {
            se_string l(StringUtils::strip_edges(f->get_line()));
            StringName name(StringUtils::split(l, ' ')[0]);

            if ((ClassDB::class_exists(name) || ScriptServer::is_global_class(name)) && !_is_class_disabled_by_feature_profile(name)) {
                TreeItem *ti = recent->create_item(root);
                ti->set_text_utf8(0, l);
                ti->set_icon(0, EditorNode::get_singleton()->get_class_icon(StringName(l), StringName(base_type)));
            }
        }

        memdelete(f);
    }

    favorites->clear();

    f = FileAccess::open(PathUtils::plus_file(EditorSettings::get_singleton()->get_project_settings_dir(),
                                 se_string("favorites.") + base_type),
            FileAccess::READ);

    favorite_list.clear();

    if (f) {

        while (!f->eof_reached()) {
            se_string l(StringUtils::strip_edges(f->get_line()));

            if (!l.empty()) {
                favorite_list.push_back(l);
            }
        }

        memdelete(f);
    }

    _save_and_update_favorite_list();

    // Restore valid window bounds or pop up at default size.
    Rect2 saved_size =
            EditorSettings::get_singleton()->get_project_metadata("dialog_bounds", "create_new_node", Rect2());
    if (saved_size != Rect2()) {
        popup(saved_size);
    } else {
        popup_centered_clamped(Size2(900, 700) * EDSCALE, 0.8f);
    }

    if (p_dont_clear) {
        search_box->select_all();
    } else {
        search_box->clear();
    }

    search_box->grab_focus();

    _update_search();

    is_replace_mode = p_replace_mode;

    if (p_replace_mode) {
        select_type(p_select_type);
        set_title(FormatSN(TTR("Change %s Type").asCString(), base_type.asCString()));
        get_ok()->set_text(TTR("Change"));
    } else {
        set_title(FormatSN(TTR("Create New %s").asCString(), base_type.asCString()));
        get_ok()->set_text(TTR("Create"));
    }
}

void CreateDialog::_text_changed(se_string_view p_newtext) {

    _update_search();
}

void CreateDialog::_sbox_input(const Ref<InputEvent> &p_ie) {

    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_ie);
    if (k && (k->get_scancode() == KEY_UP || k->get_scancode() == KEY_DOWN || k->get_scancode() == KEY_PAGEUP ||
                     k->get_scancode() == KEY_PAGEDOWN)) {

        search_options->call("_gui_input", k);
        search_box->accept_event();
    }
}

void CreateDialog::add_type(
        const StringName &p_type, HashMap<StringName, TreeItem *> &p_types, TreeItem *p_root, TreeItem **to_select) {

    if (p_types.contains(p_type)) return;

    bool cpp_type = ClassDB::class_exists(p_type);
    EditorData &ed = EditorNode::get_editor_data();

    if (p_type == base_type) return;

    if (cpp_type) {
        if (!ClassDB::is_parent_class(p_type, base_type)) return;
    } else {
        if (!search_loaded_scripts.contains(p_type)) {
            search_loaded_scripts[p_type] = ed.script_class_load_script(p_type);
        }
        if (!ScriptServer::is_global_class(p_type) || !ed.script_class_is_parent(p_type, base_type)) return;

        se_string_view script_path = ScriptServer::get_global_class_path(p_type);
        if (StringUtils::contains(script_path, "res://addons/")) {
            if (!EditorNode::get_singleton()->is_addon_plugin_enabled(
                        StringName(StringUtils::get_slice(script_path, '/', 3))))
                return;
        }
    }

    StringName inherits = cpp_type ? ClassDB::get_parent_class(p_type) : ed.script_class_get_base(p_type);

    TreeItem *parent = p_root;

    if (!inherits.empty()) {

        if (!p_types.contains(inherits)) {

            add_type(inherits, p_types, p_root, to_select);
        }

        if (p_types.contains(inherits))
            parent = p_types[inherits];
        else if (ScriptServer::is_global_class(inherits))
            return;
    }

    bool can_instance = cpp_type && ClassDB::can_instance(p_type) || ScriptServer::is_global_class(p_type);

    TreeItem *item = search_options->create_item(parent);
    if (cpp_type) {
        item->set_text(0, p_type);
    } else {
        item->set_metadata(0, p_type);
        item->set_text_utf8(
                0, se_string(p_type) + " (" + PathUtils::get_file(ScriptServer::get_global_class_path(p_type)) + ")");
    }
    if (!can_instance) {
        item->set_custom_color(0, get_color("disabled_font_color", "Editor"));
        item->set_selectable(0, false);
    } else if (!(*to_select && (*to_select)->get_text(0) == search_box->get_text())) {
        se_string search_term = StringUtils::to_utf8(search_box->get_text_ui().toLower());

        // if the node name matches exactly as the search, the node should be selected.
        // this also fixes when the user clicks on recent nodes.
        if (StringUtils::to_lower(p_type) == search_term) {
            *to_select = item;
        } else {
            bool current_type_prefered = _is_type_prefered(p_type);
            bool selected_type_prefered =
                    *to_select ? _is_type_prefered(StringName(StringUtils::split((*to_select)->get_text(0), ' ')[0])) :
                                 false;

            bool is_subsequence_of_type =
                    StringUtils::is_subsequence_of(search_term, p_type, StringUtils::CaseInsensitive);
            bool is_substring_of_type = StringUtils::contains(StringUtils::to_lower(p_type), search_term);
            bool is_substring_of_selected = false;
            bool is_subsequence_of_selected = false;
            bool is_selected_equal = false;

            if (*to_select) {
                se_string name = StringUtils::to_lower(StringUtils::split((*to_select)->get_text(0), ' ')[0]);
                is_substring_of_selected = StringUtils::contains(name, search_term);
                is_subsequence_of_selected = StringUtils::is_subsequence_of(search_term, name);
                is_selected_equal = name == search_term;
            }

            if (is_subsequence_of_type && !is_selected_equal) {
                if (is_substring_of_type) {
                    if (!is_substring_of_selected || current_type_prefered && !selected_type_prefered) {
                        *to_select = item;
                    }
                } else {
                    // substring results weigh more than subsequences, so let's make sure we don't override them
                    if (!is_substring_of_selected) {
                        if (!is_subsequence_of_selected || current_type_prefered && !selected_type_prefered) {
                            *to_select = item;
                        }
                    }
                }
            }
        }
    }

    if (bool(EditorSettings::get_singleton()->get("docks/scene_tree/start_create_dialog_fully_expanded"))) {
        item->set_collapsed(false);
    } else {
        // don't collapse search results
        bool collapse = search_box->get_text_ui().isEmpty();
        // don't collapse the root node
        collapse &= item != p_root;
        // don't collapse abstract nodes on the first tree level
        collapse &= parent != p_root || can_instance;
        item->set_collapsed(collapse);
    }

    const se_string &description = EditorHelp::get_doc_data()->class_list[p_type].brief_description;
    item->set_tooltip(0, StringName(description));

    item->set_icon(0, EditorNode::get_singleton()->get_class_icon(p_type, base_type));

    p_types[p_type] = item;
}

bool CreateDialog::_is_type_prefered(const StringName &type) {
    bool cpp_type = ClassDB::class_exists(type);
    EditorData &ed = EditorNode::get_editor_data();

    if (cpp_type) {
        return ClassDB::is_parent_class(type, preferred_search_result_type);
    }
    return ed.script_class_is_parent(type, preferred_search_result_type);
}

bool CreateDialog::_is_class_disabled_by_feature_profile(const StringName &p_class) {

    Ref<EditorFeatureProfile> profile = EditorFeatureProfileManager::get_singleton()->get_current_profile();
    if (not profile) {
        return false;
    }

    return profile->is_class_disabled(p_class);
}

void CreateDialog::select_type(const StringName &p_type) {

    TreeItem *to_select;
    if (search_options_types.contains(p_type)) {
        to_select = search_options_types[p_type];
    } else {
        to_select = search_options->get_root();
    }

    // uncollapse from selected type to top level
    // TODO: should this be in tree?
    TreeItem *cur = to_select;
    while (cur) {
        cur->set_collapsed(false);
        cur = cur->get_parent();
    }

    to_select->select(0);

    search_options->scroll_to_item(to_select);
}

void CreateDialog::_update_search() {
    search_options->clear();
    favorite->set_disabled(true);

    help_bit->set_text("");

    search_options_types.clear();

    TreeItem *root = search_options->create_item();
    EditorData &ed = EditorNode::get_editor_data();

    root->set_text(0, base_type);
    if (has_icon(base_type, "EditorIcons")) {
        root->set_icon(0, get_icon(base_type, "EditorIcons"));
    }

    TreeItem *to_select = search_box->get_text() == base_type ? root : nullptr;

    for (int i = 0, fin = type_list.size(); i < fin; ++i) {

        const StringName &type = type_list[i];

        if (_is_class_disabled_by_feature_profile(type)) {
            continue;
        }
        bool cpp_type = ClassDB::class_exists(type);

        if (base_type == "Node" && StringUtils::begins_with(type, "Editor")) continue; // do not show editor nodes

        if (cpp_type && !ClassDB::can_instance(type)) continue; // can't create what can't be instanced

        if (cpp_type) {
            bool skip = false;

            for (Set<StringName>::iterator E = type_blacklist.begin(); E != type_blacklist.end() && !skip; ++E) {
                if (ClassDB::is_parent_class(type, *E)) skip = true;
            }
            if (skip) continue;
        }

        if (search_box->get_text_ui().isEmpty()) {
            add_type(type, search_options_types, root, &to_select);
        } else {

            bool found = false;
            StringName type2 = type;

            if (!cpp_type && !search_loaded_scripts.contains(type)) {
                search_loaded_scripts[type] = ed.script_class_load_script(type);
            }
            while (!type2.empty() &&
                    (cpp_type ? ClassDB::is_parent_class(type2, base_type) :
                                ed.script_class_is_parent(type2, base_type)) &&
                    type2 != base_type) {
                if (StringUtils::is_subsequence_of(search_box->get_text(), type2, StringUtils::CaseInsensitive)) {

                    found = true;
                    break;
                }

                type2 = cpp_type ? ClassDB::get_parent_class(type2) : ed.script_class_get_base(type2);
                if (!cpp_type && !search_loaded_scripts.contains(type2)) {
                    search_loaded_scripts[type2] = ed.script_class_load_script(type2);
                }
            }

            if (found) {
                add_type(type, search_options_types, root, &to_select);
            }
        }

        if (EditorNode::get_editor_data().get_custom_types().contains(type) &&
                ClassDB::is_parent_class(type, base_type)) {
            // there are custom types based on this... cool.

            const Vector<EditorData::CustomType> &ct = EditorNode::get_editor_data().get_custom_types().at(type);
            for (int i = 0; i < ct.size(); i++) {

                bool show = StringUtils::is_subsequence_of(
                        search_box->get_text(), ct[i].name, StringUtils::CaseInsensitive);

                if (!show) continue;

                if (!search_options_types.contains(type)) add_type(type, search_options_types, root, &to_select);

                TreeItem *ti;
                if (search_options_types.contains(type))
                    ti = search_options_types[type];
                else
                    ti = search_options->get_root();

                TreeItem *item = search_options->create_item(ti);
                item->set_metadata(0, type);
                item->set_text(0, ct[i].name);
                if (ct[i].icon) {
                    item->set_icon(0, ct[i].icon);
                }

                if (!to_select || ct[i].name == search_box->get_text()) {
                    to_select = item;
                }
            }
        }
    }

    if (search_box->get_text_ui().isEmpty()) {
        to_select = root;
    }

    if (to_select) {
        to_select->select(0);
        search_options->scroll_to_item(to_select);
        favorite->set_disabled(false);
        favorite->set_pressed(favorite_list.contains(to_select->get_text(0)));
    }

    get_ok()->set_disabled(root->get_children() == nullptr);
}

void CreateDialog::_confirmed() {

    TreeItem *ti = search_options->get_selected();
    if (!ti) return;

    FileAccess *f = FileAccess::open(PathUtils::plus_file(EditorSettings::get_singleton()->get_project_settings_dir(),
                                             se_string("create_recent.") + base_type),
            FileAccess::WRITE);

    if (f) {
        f->store_line(get_selected_type());
        TreeItem *t = recent->get_root();
        if (t) t = t->get_children();
        int count = 0;
        while (t) {
            if (StringName(t->get_text(0)) != get_selected_type()) {

                f->store_line(t->get_text(0));
            }

            if (count > 32) {
                // limit it to 32 entries..
                break;
            }
            t = t->get_next();
            count++;
        }

        memdelete(f);
    }

    emit_signal("create");
    hide();
}

void CreateDialog::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            connect("confirmed", this, "_confirmed");
            search_box->set_right_icon(get_icon("Search", "EditorIcons"));
            search_box->set_clear_button_enabled(true);
            favorite->set_icon(get_icon("Favorites", "EditorIcons"));
        } break;
        case NOTIFICATION_EXIT_TREE: {
            disconnect("confirmed", this, "_confirmed");
        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {
            if (is_visible_in_tree()) {
                search_box->call_deferred("grab_focus"); // still not visible
                search_box->select_all();
            }
        } break;
        case NOTIFICATION_POPUP_HIDE: {
            EditorSettings::get_singleton()->get_project_metadata("dialog_bounds", "create_new_node", get_rect());
            search_loaded_scripts.clear();
        } break;
    }
}

void CreateDialog::set_base_type(const StringName &p_base) {

    base_type = p_base;
    if (is_replace_mode)
        set_title(FormatSN(TTR("Change %s Type").asCString(), p_base.asCString()));
    else
        set_title(FormatSN(TTR("Create New %s").asCString(), p_base.asCString()));

    _update_search();
}

StringName CreateDialog::get_base_type() const {

    return base_type;
}

void CreateDialog::set_preferred_search_result_type(const StringName &p_preferred_type) {
    preferred_search_result_type = p_preferred_type;
}

StringName CreateDialog::get_preferred_search_result_type() {

    return preferred_search_result_type;
}
StringName CreateDialog::get_selected_type() {

    TreeItem *selected = search_options->get_selected();
    if (selected)
        return StringName(selected->get_text(0));
    else
        return StringName();
}

Object *CreateDialog::instance_selected() {

    TreeItem *selected = search_options->get_selected();

    if (selected) {

        Variant md = selected->get_metadata(0);

        StringName custom;
        if (md.get_type() != VariantType::NIL) custom = md;

        if (!custom.empty()) {
            if (ScriptServer::is_global_class(custom)) {
                Object *obj = EditorNode::get_editor_data().script_class_instance(custom);
                Node *n = object_cast<Node>(obj);
                if (n) n->set_name(custom);
                return obj;
            }
            return EditorNode::get_editor_data().instance_custom_type(StringName(selected->get_text(0)), custom);
        } else {
            return ClassDB::instance(StringName(selected->get_text(0)));
        }
    }

    return nullptr;
}

void CreateDialog::_item_selected() {

    TreeItem *item = search_options->get_selected();
    if (!item) return;

    StringName name(item->get_text(0));

    favorite->set_disabled(false);
    favorite->set_pressed(favorite_list.contains(se_string(name)));

    if (!EditorHelp::get_doc_data()->class_list.contains(name)) return;

    help_bit->set_text(EditorHelp::get_doc_data()->class_list[name].brief_description);

    get_ok()->set_disabled(false);
}

void CreateDialog::_favorite_toggled() {

    TreeItem *item = search_options->get_selected();
    if (!item) return;

    se_string name = item->get_text(0);

    if (!favorite_list.contains(name)) {
        favorite_list.push_back(name);
        favorite->set_pressed(true);
    } else {
        favorite_list.erase_first(name);
        favorite->set_pressed(false);
    }

    _save_and_update_favorite_list();
}

void CreateDialog::_save_favorite_list() {

    FileAccess *f = FileAccess::open(PathUtils::plus_file(EditorSettings::get_singleton()->get_project_settings_dir(),
                                             se_string("favorites.") + base_type),
            FileAccess::WRITE);

    if (f) {

        for (int i = 0; i < favorite_list.size(); i++) {
            const se_string &l = favorite_list[i];
            StringName name(StringUtils::split(l, ' ')[0]);
            if (!(ClassDB::class_exists(name) || ScriptServer::is_global_class(name))) continue;
            f->store_line(l);
        }
        memdelete(f);
    }
}

void CreateDialog::_update_favorite_list() {

    favorites->clear();
    TreeItem *root = favorites->create_item();
    for (int i = 0; i < favorite_list.size(); i++) {
        const se_string &l = favorite_list[i];
        StringName name(StringUtils::split(l, ' ')[0]);
        if (!((ClassDB::class_exists(name) || ScriptServer::is_global_class(name)) && !_is_class_disabled_by_feature_profile(name)))
            continue;
        TreeItem *ti = favorites->create_item(root);
        ti->set_text_utf8(0, l);
        ti->set_icon(0, EditorNode::get_singleton()->get_class_icon(StringName(l), base_type));
    }
    emit_signal("favorites_updated");
}

void CreateDialog::_history_selected() {

    TreeItem *item = recent->get_selected();
    if (!item) return;

    search_box->set_text_utf8(StringUtils::get_slice(item->get_text(0), ' ', 0));
    favorites->deselect_all();
    _update_search();
}

void CreateDialog::_favorite_selected() {

    TreeItem *item = favorites->get_selected();
    if (!item) return;

    search_box->set_text_utf8(StringUtils::get_slice(item->get_text(0), ' ', 0));
    recent->deselect_all();
    _update_search();
}

void CreateDialog::_history_activated() {

    _history_selected();
    _confirmed();
}

void CreateDialog::_favorite_activated() {

    _favorite_selected();
    _confirmed();
}

Variant CreateDialog::get_drag_data_fw(const Point2 &p_point, Control *p_from) {

    TreeItem *ti = favorites->get_item_at_position(p_point);
    if (ti) {
        Dictionary d;
        d["type"] = "create_favorite_drag";
        d["class"] = ti->get_text(0);

        ToolButton *tb = memnew(ToolButton);
        tb->set_icon(ti->get_icon(0));
        tb->set_text_utf8(ti->get_text(0));
        set_drag_preview(tb);

        return d;
    }

    return Variant();
}

bool CreateDialog::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {

    Dictionary d = p_data;
    if (d.has("type") && String(d["type"]) == "create_favorite_drag") {
        favorites->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN);
        return true;
    }

    return false;
}
void CreateDialog::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {

    Dictionary d = p_data;

    TreeItem *ti = favorites->get_item_at_position(p_point);
    if (!ti) return;

    se_string drop_at = ti->get_text(0);
    int ds = favorites->get_drop_section_at_position(p_point);

    auto drop_idx = favorite_list.index_of(drop_at);
    if (drop_idx >= favorite_list.size())
        return;

    se_string type = d["class"];

    auto from_idx = favorite_list.index_of(type);
    if (from_idx >= favorite_list.size())
        return;

    if (drop_idx == from_idx) {
        ds = -1; // cause it will be gone
    } else if (drop_idx > from_idx) {
        drop_idx--;
    }

    favorite_list.erase_at(from_idx);

    if (ds < 0) {
        favorite_list.insert_at(drop_idx, type);
    } else {
        if (drop_idx >= favorite_list.size() - 1) {
            favorite_list.push_back(type);
        } else {
            favorite_list.insert_at(drop_idx + 1, type);
        }
    }

    _save_and_update_favorite_list();
}

void CreateDialog::_save_and_update_favorite_list() {
    _save_favorite_list();
    _update_favorite_list();
}

void CreateDialog::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("_text_changed"), &CreateDialog::_text_changed);
    MethodBinder::bind_method(D_METHOD("_confirmed"), &CreateDialog::_confirmed);
    MethodBinder::bind_method(D_METHOD("_sbox_input"), &CreateDialog::_sbox_input);
    MethodBinder::bind_method(D_METHOD("_item_selected"), &CreateDialog::_item_selected);
    MethodBinder::bind_method(D_METHOD("_favorite_toggled"), &CreateDialog::_favorite_toggled);
    MethodBinder::bind_method(D_METHOD("_history_selected"), &CreateDialog::_history_selected);
    MethodBinder::bind_method(D_METHOD("_favorite_selected"), &CreateDialog::_favorite_selected);
    MethodBinder::bind_method(D_METHOD("_history_activated"), &CreateDialog::_history_activated);
    MethodBinder::bind_method(D_METHOD("_favorite_activated"), &CreateDialog::_favorite_activated);
    MethodBinder::bind_method(
            D_METHOD("_save_and_update_favorite_list"), &CreateDialog::_save_and_update_favorite_list);

    MethodBinder::bind_method("get_drag_data_fw", &CreateDialog::get_drag_data_fw);
    MethodBinder::bind_method("can_drop_data_fw", &CreateDialog::can_drop_data_fw);
    MethodBinder::bind_method("drop_data_fw", &CreateDialog::drop_data_fw);

    ADD_SIGNAL(MethodInfo("create"));
    ADD_SIGNAL(MethodInfo("favorites_updated"));
}

CreateDialog::CreateDialog() {

    is_replace_mode = false;

    set_resizable(true);

    HSplitContainer *hsc = memnew(HSplitContainer);
    add_child(hsc);

    VSplitContainer *vsc = memnew(VSplitContainer);
    hsc->add_child(vsc);

    VBoxContainer *fav_vb = memnew(VBoxContainer);
    vsc->add_child(fav_vb);
    fav_vb->set_custom_minimum_size(Size2(150, 100) * EDSCALE);
    fav_vb->set_v_size_flags(SIZE_EXPAND_FILL);

    favorites = memnew(Tree);
    fav_vb->add_margin_child(TTR("Favorites:"), favorites, true);
    favorites->set_hide_root(true);
    favorites->set_hide_folding(true);
    favorites->set_allow_reselect(true);
    favorites->connect("cell_selected", this, "_favorite_selected");
    favorites->connect("item_activated", this, "_favorite_activated");
    favorites->set_drag_forwarding(this);
    favorites->add_constant_override("draw_guides", 1);

    VBoxContainer *rec_vb = memnew(VBoxContainer);
    vsc->add_child(rec_vb);
    rec_vb->set_custom_minimum_size(Size2(150, 100) * EDSCALE);
    rec_vb->set_v_size_flags(SIZE_EXPAND_FILL);

    recent = memnew(Tree);
    rec_vb->add_margin_child(TTR("Recent:"), recent, true);
    recent->set_hide_root(true);
    recent->set_hide_folding(true);
    recent->set_allow_reselect(true);
    recent->connect("cell_selected", this, "_history_selected");
    recent->connect("item_activated", this, "_history_activated");
    recent->add_constant_override("draw_guides", 1);

    VBoxContainer *vbc = memnew(VBoxContainer);
    hsc->add_child(vbc);
    vbc->set_custom_minimum_size(Size2(300, 0) * EDSCALE);
    vbc->set_h_size_flags(SIZE_EXPAND_FILL);
    HBoxContainer *search_hb = memnew(HBoxContainer);
    search_box = memnew(LineEdit);
    search_box->set_h_size_flags(SIZE_EXPAND_FILL);
    search_hb->add_child(search_box);
    favorite = memnew(Button);
    favorite->set_flat(true);
    favorite->set_toggle_mode(true);
    search_hb->add_child(favorite);
    favorite->connect("pressed", this, "_favorite_toggled");
    vbc->add_margin_child(TTR("Search:"), search_hb);
    search_box->connect("text_changed", this, "_text_changed");
    search_box->connect("gui_input", this, "_sbox_input");
    search_options = memnew(Tree);
    vbc->add_margin_child(TTR("Matches:"), search_options, true);
    get_ok()->set_disabled(true);
    register_text_enter(search_box);
    set_hide_on_ok(false);
    search_options->connect("item_activated", this, "_confirmed");
    search_options->connect("cell_selected", this, "_item_selected");
    base_type = "Object";
    preferred_search_result_type = "";

    help_bit = memnew(EditorHelpBit);
    vbc->add_margin_child(TTR("Description:"), help_bit);
    help_bit->connect("request_hide", this, "_closed");

    type_blacklist.insert("PluginScript"); // PluginScript must be initialized before use, which is not possible here
    type_blacklist.insert("ScriptCreateDialog"); // This is an exposed editor Node that doesn't have an Editor prefix.
}
