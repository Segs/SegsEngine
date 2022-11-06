/*************************************************************************/
/*  import_defaults_editor.cpp                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "import_defaults_editor.h"

#include "core/class_db.h"
#include "core/project_settings.h"
#include "core/object_tooling.h"
#include "core/callable_method_pointer.h"
#include "core/undo_redo.h"
#include "editor/editor_data.h"
#include "editor/editor_plugin_settings.h"
#include "editor/editor_sectioned_inspector.h"
#include "editor/editor_settings.h"
#include "editor_autoload_settings.h"
#include "scene/gui/center_container.h"
#include "scene/gui/box_container.h"
#include "scene/gui/option_button.h"

#include "EASTL/sort.h"


IMPL_GDCLASS(ImportDefaultsEditor)

class ImportDefaultsEditorSettings : public Object {
    GDCLASS(ImportDefaultsEditorSettings, Object)
    friend class ImportDefaultsEditor;
    List<PropertyInfo> properties;
    HashMap<StringName, Variant> values;
    HashMap<StringName, Variant> default_values;

    ResourceImporterInterface *importer;

protected:
    bool _set(const StringName &p_name, const Variant &p_value) {
        if (values.contains(p_name)) {
            values[p_name] = p_value;
            return true;
        } else {
            return false;
        }
    }
    bool _get(const StringName &p_name, Variant &r_ret) const {
        auto iter = values.find(p_name);
        if (iter!=values.end()) {
            r_ret = iter->second;
            return true;
        } else {
            r_ret = Variant();
            return false;
        }
    }
    void _get_property_list(Vector<PropertyInfo> *p_list) const {
        if (!importer) {
            return;
        }
        for (const PropertyInfo & E : properties) {
            if (importer->get_option_visibility(E.name, values)) {
                p_list->emplace_back(E);
            }
        }
    }
};
IMPL_GDCLASS(ImportDefaultsEditorSettings)


void ImportDefaultsEditor::_notification(int p_what) {
    switch (p_what) {
    case NOTIFICATION_ENTER_TREE:
    case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
        inspector->set_property_name_style(EditorPropertyNameProcessor::get_settings_style());
    } break;

    case NOTIFICATION_PREDELETE: {
        inspector->edit(nullptr);
    } break;
    }
}

void ImportDefaultsEditor::_reset() {
    if (settings->importer) {
        settings->values = settings->default_values;
        Object_change_notify(settings);
    }
}

void ImportDefaultsEditor::_save() {
    if (settings->importer) {
        Dictionary modified;

        for (const auto & E : settings->values) {
            if (E.second != settings->default_values[E.first]) {
                modified[E.first] = E.second;
            }
        }

        if (modified.size()) {
            ProjectSettings::get_singleton()->set(StringName("importer_defaults/") + settings->importer->get_importer_name(), modified);
        } else {
            ProjectSettings::get_singleton()->set(StringName("importer_defaults/") + settings->importer->get_importer_name(), Variant());
        }

        // Calling ProjectSettings::set() causes the signal "project_settings_changed" to be sent to ProjectSettings.
        // ProjectSettingsEditor subscribes to this and can reads the settings updated here.
    }
}

void ImportDefaultsEditor::_update_importer() {
    Vector<ResourceImporterInterface * > importer_list;
    ResourceFormatImporter::get_singleton()->get_importers(&importer_list);
    ResourceImporterInterface * importer=nullptr;
    for (ResourceImporterInterface * E : importer_list) {
        if (E->get_visible_name() == importers->get_item_text(importers->get_selected())) {
            importer = E;
            break;
        }
    }

    settings->properties.clear();
    settings->values.clear();
    settings->importer = importer;

    if (importer) {
        Vector<ResourceImporterInterface::ImportOption> options;
        importer->get_import_options(&options);
        Dictionary d;
        if (ProjectSettings::get_singleton()->has_setting(StringName("importer_defaults/") + importer->get_importer_name())) {
            d = ProjectSettings::get_singleton()->get(StringName("importer_defaults/") + importer->get_importer_name()).as<Dictionary>();
        }

        for (const ResourceImporterInterface::ImportOption &E : options) {
            settings->properties.push_back(E.option);
            if (d.has(E.option.name)) {
                settings->values[E.option.name] = d[E.option.name];
            } else {
                settings->values[E.option.name] = E.default_value;
            }
            settings->default_values[E.option.name] = E.default_value;
        }

        save_defaults->set_disabled(false);
        reset_defaults->set_disabled(false);

    } else {
        save_defaults->set_disabled(true);
        reset_defaults->set_disabled(true);
    }
    Object_change_notify(settings);

    inspector->edit(settings);
}

void ImportDefaultsEditor::_importer_selected(int p_index) {
    _update_importer();
}

void ImportDefaultsEditor::clear() {
    String last_selected;
    if (importers->get_selected() > 0) {
        last_selected = importers->get_item_text(importers->get_selected());
    }

    importers->clear();

    importers->add_item("<" + TTR("Select Importer") + ">");
    importers->set_item_disabled(0, true);

    Vector<ResourceImporterInterface *> importer_list;
    ResourceFormatImporter::get_singleton()->get_importers(&importer_list);
    Vector<String> names;
    for (ResourceImporterInterface *E : importer_list) {
        String vn = E->get_visible_name();
        names.push_back(vn);
    }
    eastl::sort(names.begin(),names.end());

    for (int i = 0; i < names.size(); i++) {
        importers->add_item(StringName(names[i]));

        if (names[i] == last_selected) {
            importers->select(i + 1);
        }
    }
}

void ImportDefaultsEditor::_bind_methods() {
//    Metho::bind_method(D_METHOD("_reset"), &ImportDefaultsEditor::_reset);
//    ClassDB::bind_method(D_METHOD("_save"), &ImportDefaultsEditor::_save);
//    ClassDB::bind_method(D_METHOD("_importer_selected"), &ImportDefaultsEditor::_importer_selected);

    ADD_SIGNAL(MethodInfo("project_settings_changed"));
    ImportDefaultsEditorSettings::initialize_class();
}

ImportDefaultsEditor::ImportDefaultsEditor() {
    HBoxContainer *hb = memnew(HBoxContainer);
    hb->add_child(memnew(Label(TTR("Importer:"))));
    importers = memnew(OptionButton);
    hb->add_child(importers);
    hb->add_spacer();
    importers->connect("item_selected", callable_mp(this, &ImportDefaultsEditor::_importer_selected));
    reset_defaults = memnew(Button);
    reset_defaults->set_text(TTR("Reset to Defaults"));
    reset_defaults->set_disabled(true);
    reset_defaults->connect("pressed", callable_mp(this, &ImportDefaultsEditor::_reset));
    hb->add_child(reset_defaults);
    add_child(hb);
    inspector = memnew(EditorInspector);
    add_child(inspector);
    inspector->set_v_size_flags(SIZE_EXPAND_FILL);
    CenterContainer *cc = memnew(CenterContainer);
    save_defaults = memnew(Button);
    save_defaults->set_text(TTR("Save"));
    save_defaults->connect("pressed", callable_mp(this, &ImportDefaultsEditor::_save));
    cc->add_child(save_defaults);
    add_child(cc);

    settings = memnew(ImportDefaultsEditorSettings);
}

ImportDefaultsEditor::~ImportDefaultsEditor() {
    memdelete(settings);
}
