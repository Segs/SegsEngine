/*************************************************************************/
/*  gdnative_library_editor_plugin.cpp                                   */
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

#ifdef TOOLS_ENABLED
#include "gdnative_library_editor_plugin.h"

#include "core/method_bind.h"
#include "core/translation_helpers.h"
#include "core/string.h"
#include "editor/editor_scale.h"
#include "gdnative.h"
IMPL_GDCLASS(GDNativeLibraryEditor)
IMPL_GDCLASS(GDNativeLibraryEditorPlugin)

void GDNativeLibraryEditor::edit(Ref<GDNativeLibrary> p_library) {
    library = p_library;
    Ref<ConfigFile> config = p_library->get_config_file();

    for (eastl::pair<const String,NativePlatformConfig> &E : platforms) {
        for (List<String>::Element *it = E.second.entries.front(); it; it = it->next()) {

            String target = E.first + "." + it->deref();
            String target_utf8 = StringUtils::to_utf8(target).data();
            TargetConfig ecfg;
            ecfg.library = config->get_value(("entry"), target_utf8, "");
            ecfg.dependencies = config->get_value(("dependencies"), target_utf8, Array());
            entry_configs[target] = ecfg;
        }
    }

    _update_tree();
}

void GDNativeLibraryEditor::_bind_methods() {

    MethodBinder::bind_method("_on_item_button", &GDNativeLibraryEditor::_on_item_button);
    MethodBinder::bind_method("_on_library_selected", &GDNativeLibraryEditor::_on_library_selected);
    MethodBinder::bind_method("_on_dependencies_selected", &GDNativeLibraryEditor::_on_dependencies_selected);
    MethodBinder::bind_method("_on_filter_selected", &GDNativeLibraryEditor::_on_filter_selected);
    MethodBinder::bind_method("_on_item_collapsed", &GDNativeLibraryEditor::_on_item_collapsed);
    MethodBinder::bind_method("_on_item_activated", &GDNativeLibraryEditor::_on_item_activated);
    MethodBinder::bind_method("_on_create_new_entry", &GDNativeLibraryEditor::_on_create_new_entry);
}

void GDNativeLibraryEditor::_update_tree() {

    tree->clear();
    TreeItem *root = tree->create_item();

    PopupMenu *filter_list = filter->get_popup();
    String text;
    for (int i = 0; i < filter_list->get_item_count(); i++) {

        if (!filter_list->is_item_checked(i)) {
            continue;
        }
        Map<String, NativePlatformConfig>::iterator E = platforms.find(filter_list->get_item_metadata(i));
        if (!text.empty()) {
            text += String(", ");
        }
        text += E->second.name;

        TreeItem *platform = tree->create_item(root);
        platform->set_text(0, E->second.name);
        platform->set_metadata(0, E->second.library_extension);

        platform->set_custom_bg_color(0, get_color("prop_category", "Editor"));
        platform->set_custom_bg_color(1, get_color("prop_category", "Editor"));
        platform->set_custom_bg_color(2, get_color("prop_category", "Editor"));
        platform->set_selectable(0, false);
        platform->set_expand_right(0, true);

        for (List<String>::Element *it = E->second.entries.front(); it; it = it->next()) {

            String target = E->first + "." + it->deref();
            TreeItem *bit = tree->create_item(platform);

            bit->set_text(0, it->deref());
            bit->set_metadata(0, target);
            bit->set_selectable(0, false);
            bit->set_custom_bg_color(0, get_color("prop_subsection", "Editor"));

            bit->add_button(1, get_icon("Folder", "EditorIcons"), BUTTON_SELECT_LIBRARY, false, TTR(String("Select the dynamic library for this entry")));
            String file = entry_configs[target].library;
            if (!file.empty()) {
                bit->add_button(1, get_icon("Clear", "EditorIcons"), BUTTON_CLEAR_LIBRARY, false, TTR(String("Clear")));
            }
            bit->set_text(1, file);

            bit->add_button(2, get_icon("Folder", "EditorIcons"), BUTTON_SELECT_DEPENDENCES, false, TTR(String("Select dependencies of the library for this entry")));
            Array files = entry_configs[target].dependencies;
            if (files.size()) {
                bit->add_button(2, get_icon("Clear", "EditorIcons"), BUTTON_CLEAR_DEPENDENCES, false, TTR(String("Clear")));
            }
            bit->set_text(2, Variant(files));

            bit->add_button(3, get_icon("MoveUp", "EditorIcons"), BUTTON_MOVE_UP, false, TTR(String("Move Up")));
            bit->add_button(3, get_icon("MoveDown", "EditorIcons"), BUTTON_MOVE_DOWN, false, TTR(String("Move Down")));
            bit->add_button(3, get_icon("Remove", "EditorIcons"), BUTTON_ERASE_ENTRY, false, TTR(String("Remove current entry")));
        }

        TreeItem *new_arch = tree->create_item(platform);
        new_arch->set_text(0, TTR(String("Double click to create a new entry")));
        new_arch->set_text_align(0, TreeItem::ALIGN_CENTER);
        new_arch->set_custom_color(0, get_color("accent_color", "Editor"));
        new_arch->set_expand_right(0, true);
        new_arch->set_metadata(1, E->first);

        platform->set_collapsed(collapsed_items.contains(E->second.name));
    }
    filter->set_text(text);
}

void GDNativeLibraryEditor::_on_item_button(Object *item, int column, int id) {

    String target = object_cast<TreeItem>(item)->get_metadata(0);
    String platform = StringUtils::substr(target,0, StringUtils::find(target,"."));
    String entry = StringUtils::substr(target,platform.length() + 1, target.length());
    String section((id == BUTTON_SELECT_DEPENDENCES || id == BUTTON_CLEAR_DEPENDENCES) ? "dependencies" : "entry");

    if (id == BUTTON_SELECT_LIBRARY || id == BUTTON_SELECT_DEPENDENCES) {

        EditorFileDialog::Mode mode = EditorFileDialog::MODE_OPEN_FILE;
        if (id == BUTTON_SELECT_DEPENDENCES)
            mode = EditorFileDialog::MODE_OPEN_FILES;

        file_dialog->set_meta("target", target);
        file_dialog->set_meta("section", section);
        file_dialog->clear_filters();
        file_dialog->add_filter(object_cast<TreeItem>(item)->get_parent()->get_metadata(0));
        file_dialog->set_mode(mode);
        file_dialog->popup_centered_ratio();

    } else if (id == BUTTON_CLEAR_LIBRARY) {
        _set_target_value(section, target, "");
    } else if (id == BUTTON_CLEAR_DEPENDENCES) {
        _set_target_value(section, target, Array());
    } else if (id == BUTTON_ERASE_ENTRY) {
        _erase_entry(platform, entry);
    } else if (id == BUTTON_MOVE_UP || id == BUTTON_MOVE_DOWN) {
        _move_entry(platform, entry, id);
    }
}

void GDNativeLibraryEditor::_on_library_selected(const String &file) {

    _set_target_value(file_dialog->get_meta("section"), file_dialog->get_meta("target"), file);
}

void GDNativeLibraryEditor::_on_dependencies_selected(const PoolStringArray &files) {

    _set_target_value(file_dialog->get_meta("section"), file_dialog->get_meta("target"), files);
}

void GDNativeLibraryEditor::_on_filter_selected(int index) {

    PopupMenu *filter_list = filter->get_popup();
    filter_list->set_item_checked(index, !filter_list->is_item_checked(index));
    _update_tree();
}

void GDNativeLibraryEditor::_on_item_collapsed(Object *p_item) {

    TreeItem *item = object_cast<TreeItem>(p_item);
    String name = item->get_text(0);

    if (item->is_collapsed()) {
        collapsed_items.insert(name);
        return;
    }
    Set<String>::iterator e = collapsed_items.find(name);
    if (e!=collapsed_items.end()) {
        collapsed_items.erase(e);
    }
}

void GDNativeLibraryEditor::_on_item_activated() {

    TreeItem *item = tree->get_selected();
    if (item && tree->get_selected_column() == 0 && item->get_metadata(0).get_type() == VariantType::NIL) {
        new_architecture_dialog->set_meta("platform", item->get_metadata(1));
        new_architecture_dialog->popup_centered();
    }
}

void GDNativeLibraryEditor::_on_create_new_entry() {

    String platform = new_architecture_dialog->get_meta("platform");
    String entry = StringUtils::strip_edges(new_architecture_input->get_text());
    if (!entry.empty()) {
        platforms[platform].entries.push_back(entry);
        _update_tree();
    }
}

void GDNativeLibraryEditor::_set_target_value(const String &section, const String &target, Variant file) {
    if (section == "entry")
        entry_configs[target].library = file;
    else if (section == "dependencies")
        entry_configs[target].dependencies = file;
    _translate_to_config_file();
    _update_tree();
}

void GDNativeLibraryEditor::_erase_entry(const String &platform, const String &entry) {

    if (platforms.contains(platform)) {
        if (List<String>::Element *E = platforms[platform].entries.find(entry)) {

            String target = platform + "." + entry;
            Ref<ConfigFile> config = library->get_config_file();

            platforms[platform].entries.erase(E);
            _set_target_value(String("entry"), target, "");
            _set_target_value(String("dependencies"), target, Array());
            _translate_to_config_file();
            _update_tree();
        }
    }
}

void GDNativeLibraryEditor::_move_entry(const String &platform, const String &entry, int dir) {
    if (List<String>::Element *E = platforms[platform].entries.find(entry)) {
        if (E->prev() && dir == BUTTON_MOVE_UP) {
            platforms[platform].entries.insert_before(E->prev(), E->deref());
            platforms[platform].entries.erase(E);
        } else if (E->next() && dir == BUTTON_MOVE_DOWN) {
            platforms[platform].entries.insert_after(E->next(), E->deref());
            platforms[platform].entries.erase(E);
        }
        _translate_to_config_file();
        _update_tree();
    }
}

void GDNativeLibraryEditor::_translate_to_config_file() {

    if (library) {

        Ref<ConfigFile> config = library->get_config_file();
        config->erase_section(String("entry"));
        config->erase_section(String("dependencies"));

        for (eastl::pair<const String,NativePlatformConfig> &E : platforms) {
            for (List<String>::Element *it = E.second.entries.front(); it; it = it->next()) {

                String target = E.first + "." + it->deref();

                if (entry_configs[target].library.empty() && entry_configs[target].dependencies.empty())
                    continue;
                String target_utf8 = StringUtils::to_utf8(target).data();

                config->set_value("entry", target_utf8, entry_configs[target].library);
                config->set_value("dependencies", target_utf8, entry_configs[target].dependencies);
            }
        }

        library->_change_notify();
    }
}

GDNativeLibraryEditor::GDNativeLibraryEditor() {

    { // Define platforms
        NativePlatformConfig platform_windows;
        platform_windows.name = "Windows";
        platform_windows.entries.push_back(String("64"));
        platform_windows.entries.push_back(String("32"));
        platform_windows.library_extension = "*.dll";
        platforms[String("Windows")] = platform_windows;

        NativePlatformConfig platform_linux;
        platform_linux.name = "Linux/X11";
        platform_linux.entries.push_back(String("64"));
        platform_linux.entries.push_back(String("32"));
        platform_linux.library_extension = "*.so";
        platforms[String("X11")] = platform_linux;

        NativePlatformConfig platform_osx;
        platform_osx.name = "Mac OSX";
        platform_osx.entries.push_back(String("64"));
        platform_osx.entries.push_back(String("32"));
        platform_osx.library_extension = "*.dylib";
        platforms[String("OSX")] = platform_osx;

    }

    VBoxContainer *container = memnew(VBoxContainer);
    add_child(container);
    container->set_anchors_and_margins_preset(PRESET_WIDE);

    HBoxContainer *hbox = memnew(HBoxContainer);
    container->add_child(hbox);
    Label *label = memnew(Label);
    label->set_text(TTR(String("Platform:")));
    hbox->add_child(label);
    filter = memnew(MenuButton);
    filter->set_h_size_flags(SIZE_EXPAND_FILL);
    filter->set_text_align(filter->ALIGN_LEFT);
    hbox->add_child(filter);
    PopupMenu *filter_list = filter->get_popup();
    filter_list->set_hide_on_checkable_item_selection(false);

    int idx = 0;
    for (eastl::pair<const String,NativePlatformConfig> &E : platforms) {
        filter_list->add_check_item(E.second.name, idx);
        filter_list->set_item_metadata(idx, E.first);
        filter_list->set_item_checked(idx, true);
        idx += 1;
    }
    filter_list->connect("index_pressed", this, "_on_filter_selected");

    tree = memnew(Tree);
    container->add_child(tree);
    tree->set_v_size_flags(SIZE_EXPAND_FILL);
    tree->set_hide_root(true);
    tree->set_column_titles_visible(true);
    tree->set_columns(4);
    tree->set_column_expand(0, false);
    tree->set_column_min_width(0, int(200 * EDSCALE));
    tree->set_column_title(0, TTR("Platform"));
    tree->set_column_title(1, TTR("Dynamic Library"));
    tree->set_column_title(2, TTR("Dependencies"));
    tree->set_column_expand(3, false);
    tree->set_column_min_width(3, int(110 * EDSCALE));
    tree->connect("button_pressed", this, "_on_item_button");
    tree->connect("item_collapsed", this, "_on_item_collapsed");
    tree->connect("item_activated", this, "_on_item_activated");

    file_dialog = memnew(EditorFileDialog);
    file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
    file_dialog->set_resizable(true);
    add_child(file_dialog);
    file_dialog->connect("file_selected", this, "_on_library_selected");
    file_dialog->connect("files_selected", this, "_on_dependencies_selected");

    new_architecture_dialog = memnew(ConfirmationDialog);
    add_child(new_architecture_dialog);
    new_architecture_dialog->set_title(TTR("Add an architecture entry"));
    new_architecture_input = memnew(LineEdit);
    new_architecture_dialog->add_child(new_architecture_input);
    new_architecture_dialog->set_custom_minimum_size(Vector2(300, 80) * EDSCALE);
    new_architecture_input->set_anchors_and_margins_preset(PRESET_HCENTER_WIDE, PRESET_MODE_MINSIZE, 5 * EDSCALE);
    new_architecture_dialog->get_ok()->connect("pressed", this, "_on_create_new_entry");
}

void GDNativeLibraryEditorPlugin::edit(Object *p_node) {

    Ref<GDNativeLibrary> new_library(object_cast<GDNativeLibrary>(p_node));
    if (new_library)
        library_editor->edit(new_library);
}

bool GDNativeLibraryEditorPlugin::handles(Object *p_node) const {

    return p_node->is_class("GDNativeLibrary");
}

void GDNativeLibraryEditorPlugin::make_visible(bool p_visible) {

    if (p_visible) {
        button->show();
        EditorNode::get_singleton()->make_bottom_panel_item_visible(library_editor);

    } else {
        if (library_editor->is_visible_in_tree())
            EditorNode::get_singleton()->hide_bottom_panel();
        button->hide();
    }
}

GDNativeLibraryEditorPlugin::GDNativeLibraryEditorPlugin(EditorNode *p_node) {

    library_editor = memnew(GDNativeLibraryEditor);
    library_editor->set_custom_minimum_size(Size2(0, 250 * EDSCALE));
    button = p_node->add_bottom_panel_item(TTR("GDNativeLibrary"), library_editor);
    button->hide();
}

#endif
