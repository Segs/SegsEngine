/*************************************************************************/
/*  quick_open.cpp                                                       */
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

#include "quick_open.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/os/keyboard.h"
#include "core/translation_helpers.h"
#include "core/ustring.h"
#include "scene/resources/texture.h"

IMPL_GDCLASS(EditorQuickOpen)

void EditorQuickOpen::popup_dialog(const StringName &p_base, bool p_enable_multi, bool p_dontclear) {
    base_type = p_base;
    search_options->set_select_mode(p_enable_multi ? Tree::SELECT_MULTI : Tree::SELECT_SINGLE);
    popup_centered_ratio(0.4);

    if (p_dontclear)
        search_box->select_all();
    else
        search_box->clear();
    search_box->grab_focus();
    _update_search();
}

String EditorQuickOpen::get_selected() const {

    TreeItem *ti = search_options->get_selected();
    if (!ti)
        return String();

    return "res://" + ti->get_text(0);
}

Vector<String> EditorQuickOpen::get_selected_files() const {

    Vector<String> files;

    TreeItem *item = search_options->get_next_selected(search_options->get_root());
    while (item) {

        files.push_back("res://" + item->get_text(0));

        item = search_options->get_next_selected(item);
    }

    return files;
}

void EditorQuickOpen::_text_changed(StringView p_newtext) {

    _update_search();
}

void EditorQuickOpen::_sbox_input(const Ref<InputEvent> &p_ie) {

    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_ie);
    if (k) {

        switch (k->get_keycode()) {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_PAGEUP:
            case KEY_PAGEDOWN: {

                search_options->call_va("_gui_input", k);
                search_box->accept_event();

                TreeItem *root = search_options->get_root();
                if (!root->get_children())
                    break;

                TreeItem *current = search_options->get_selected();

                TreeItem *item = search_options->get_next_selected(root);
                while (item) {
                    item->deselect(0);
                    item = search_options->get_next_selected(item);
                }

                current->select(0);

            } break;
        }
    }
}

float EditorQuickOpen::_path_cmp(StringView search, StringView path) const {
    // Exact match.

    if (search == path) {
        return 1.2f;
    }
    // Substring match, with positive bias for matches close to the end of the path.
    auto pos = StringUtils::rfindn(path,search);
    if (pos != String::npos) {
        return 1.1f + 0.09 / (path.length() - pos + 1);
    }

    return StringUtils::similarity(StringUtils::to_lower(path),StringUtils::to_lower(search));
}


void EditorQuickOpen::_parse_fs(EditorFileSystemDirectory *efsd, Vector<Pair<String, Ref<Texture> > > &list) {
    for (int i = 0; i < efsd->get_subdir_count(); i++) {
        _parse_fs(efsd->get_subdir(i), list);
    }

    String search_text = search_box->get_text();
    Vector<String> base_types = String(base_type).split(',');

    for (int i = 0; i < efsd->get_file_count(); i++) {

        String file = efsd->get_file_path(i);
        file = file.substr(6, file.length());

        UIString filename = UIString::fromUtf8(file.c_str());
        StringName file_type = efsd->get_file_type(i);
        // Iterate all possible base types.
        for (int j = 0; j < base_types.size(); j++) {
            if (ClassDB::is_parent_class(file_type, StringName(base_types[j])) &&
                    StringUtils::from_utf8(search_text).contains(filename, Qt::CaseInsensitive)) {
            Pair<String, Ref<Texture> > pair;
            pair.first = file;
            StringName icon_name = search_options->has_icon(file_type, ei) ? file_type : ot;
            pair.second = search_options->get_theme_icon(icon_name, ei);
            list.push_back(pair);
                // Stop testing base types as soon as we got a match.
                break;
            }
        }
    }
}

Vector<Pair<String, Ref<Texture> > > EditorQuickOpen::_sort_fs(Vector<Pair<String, Ref<Texture> > > &list) {

    String search_text = search_box->get_text();
    Vector<Pair<String, Ref<Texture> > > sorted_list;

    if (search_text.empty() || list.empty())
        return list;

    Vector<float> scores;
    scores.resize(list.size());
    for (int i = 0; i < list.size(); i++)
        scores[i] = _path_cmp(search_text, list[i].first);

    while (!list.empty()) {

        float best_score = 0.0f;
        int best_idx = 0;

        for (int i = 0; i < list.size(); i++) {
            float current_score = scores[i];
            if (current_score > best_score) {
                best_score = current_score;
                best_idx = i;
            }
        }

        sorted_list.push_back(list[best_idx]);
        list.erase_at(best_idx);
        scores.erase_at(best_idx);
    }

    return sorted_list;
}

void EditorQuickOpen::_update_search() {

    search_options->clear();
    TreeItem *root = search_options->create_item();
    EditorFileSystemDirectory *efsd = EditorFileSystem::get_singleton()->get_filesystem();
    Vector<Pair<String, Ref<Texture> > > list;

    _parse_fs(efsd, list);
    list = _sort_fs(list);

    for (int i = 0; i < list.size(); i++) {
        TreeItem *ti = search_options->create_item(root);
        ti->set_text_utf8(0, list[i].first);
        ti->set_icon(0, list[i].second);
    }

    if (root->get_children()) {
        TreeItem *ti = root->get_children();

        ti->select(0);
        ti->set_as_cursor(0);
    }

    get_ok()->set_disabled(root->get_children() == nullptr);
}

void EditorQuickOpen::_confirmed() {

    TreeItem *ti = search_options->get_selected();
    if (!ti)
        return;
    emit_signal("quick_open");
    hide();
}

void EditorQuickOpen::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            connect("confirmed",callable_mp(this, &ClassName::_confirmed));

            search_box->set_clear_button_enabled(true);
            [[fallthrough]];
        }
        case NOTIFICATION_THEME_CHANGED: {
            search_box->set_right_icon(get_theme_icon("Search", "EditorIcons"));
        } break;
        case NOTIFICATION_EXIT_TREE: {
            disconnect("confirmed",callable_mp(this, &ClassName::_confirmed));
        } break;
    }
}

StringName EditorQuickOpen::get_base_type() const {

    return base_type;
}

void EditorQuickOpen::_bind_methods() {

    ADD_SIGNAL(MethodInfo("quick_open"));
}

EditorQuickOpen::EditorQuickOpen() {

    VBoxContainer *vbc = memnew(VBoxContainer);
    add_child(vbc);
    search_box = memnew(LineEdit);
    search_box->connect("text_changed",callable_mp(this, &ClassName::_text_changed));
    search_box->connect("gui_input",callable_mp(this, &ClassName::_sbox_input));
    vbc->add_margin_child(TTR("Search:"), search_box);

    search_options = memnew(Tree);
    search_options->connect("item_activated",callable_mp(this, &ClassName::_confirmed));
    search_options->set_hide_root(true);
    search_options->set_hide_folding(true);
    search_options->add_constant_override("draw_guides", 1);

    vbc->add_margin_child(TTR("Matches:"), search_options, true);
    get_ok()->set_text(TTR("Open"));
    get_ok()->set_disabled(true);
    register_text_enter(search_box);
    set_hide_on_ok(false);
    ei = "EditorIcons";
    ot = "Object";
}

EditorQuickOpen::~EditorQuickOpen()
{
}
