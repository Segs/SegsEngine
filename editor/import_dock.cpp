/*************************************************************************/
/*  import_dock.cpp                                                      */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "import_dock.h"
#include "editor_node.h"
#include "editor_resource_preview.h"
#include "core/method_bind.h"
#include "core/string_formatter.h"
#include "core/project_settings.h"
#include "scene/resources/style_box.h"

IMPL_GDCLASS(ImportDock)

class ImportDockParameters : public Object {
    GDCLASS(ImportDockParameters,Object)

public:
    Map<StringName, Variant> values;
    List<PropertyInfo> properties;
    ResourceImporterInterface *importer;
    Vector<se_string> paths;
    Set<StringName> checked;
    bool checking;

    bool _set(const StringName &p_name, const Variant &p_value) {

        if (values.contains(p_name)) {
            values[p_name] = p_value;
            if (checking) {
                checked.insert(p_name);
                _change_notify(p_name);
            }
            return true;
        }

        return false;
    }

    bool _get(const StringName &p_name, Variant &r_ret) const {

        if (values.contains(p_name)) {
            r_ret = values.at(p_name);
            return true;
        }

        return false;
    }
    void _get_property_list(ListPOD<PropertyInfo> *p_list) const {

        for (const List<PropertyInfo>::Element *E = properties.front(); E; E = E->next()) {
            if (!importer->get_option_visibility(E->deref().name, values))
                continue;
            PropertyInfo pi = E->deref();
            if (checking) {
                pi.usage |= PROPERTY_USAGE_CHECKABLE;
                if (checked.contains(E->deref().name)) {
                    pi.usage |= PROPERTY_USAGE_CHECKED;
                }
            }
            p_list->push_back(pi);
        }
    }

    void update() {
        _change_notify();
    }

    ImportDockParameters() {
        checking = false;
    }
};

IMPL_GDCLASS(ImportDockParameters)

void register_import_dock_classes()
{
    ImportDockParameters::initialize_class();
}
void ImportDock::set_edit_path(se_string_view p_path) {

    Ref<ConfigFile> config(make_ref_counted<ConfigFile>());
    Error err = config->load(se_string(p_path) + ".import");
    if (err != OK) {
        clear();
        return;
    }

    params->importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(config->get_value("remap", "importer").as<se_string>());
    if (params->importer==nullptr) {
        clear();
        return;
    }

    _update_options(config);

    Vector<ResourceImporterInterface *> importers;
    ResourceFormatImporter::get_singleton()->get_importers_for_extension(PathUtils::get_extension(p_path), &importers);
    ListPOD<Pair<StringName, StringName> > importer_names;

    for (int i=0,fin=importers.size(); i<fin; ++i) {
        importer_names.push_back(Pair<StringName, StringName>(importers[i]->get_visible_name(), importers[i]->get_importer_name()));
    }

    importer_names.sort(PairSort<StringName, StringName>());

    import_as->clear();

    for (const Pair<StringName, StringName> &E : importer_names) {
        import_as->add_item(E.first);
        import_as->set_item_metadata(import_as->get_item_count() - 1, E.second);
        if (E.second == params->importer->get_importer_name()) {
            import_as->select(import_as->get_item_count() - 1);
        }
    }

    params->paths.clear();
    params->paths.emplace_back(p_path);
    import->set_disabled(false);
    import_as->set_disabled(false);
    preset->set_disabled(false);

    imported->set_text(StringName(PathUtils::get_file(p_path)));
}

void ImportDock::_update_options(const Ref<ConfigFile> &p_config) {

    ListPOD<ResourceImporter::ImportOption> options;
    params->importer->get_import_options(&options);

    params->properties.clear();
    params->values.clear();
    params->checking = false;
    params->checked.clear();

    for (const ResourceImporter::ImportOption &E : options) {

        params->properties.push_back(E.option);
        if (p_config && p_config->has_section_key("params", E.option.name.asCString())) {
            params->values[E.option.name] = p_config->get_value("params", E.option.name.asCString());
        } else {
            params->values[E.option.name] = E.default_value;
        }
    }

    params->update();

    preset->get_popup()->clear();

    if (params->importer->get_preset_count() == 0) {
        preset->get_popup()->add_item(TTR("Default"));
    } else {
        for (int i = 0; i < params->importer->get_preset_count(); i++) {
            preset->get_popup()->add_item(params->importer->get_preset_name(i));
        }
    }

    preset->get_popup()->add_separator();
    preset->get_popup()->add_item(FormatSN(TTR("Set as Default for '%s'").asCString(), params->importer->get_visible_name().asCString()), ITEM_SET_AS_DEFAULT);
    if (ProjectSettings::get_singleton()->has_setting(StringName(se_string("importer_defaults/") + params->importer->get_importer_name()))) {
        preset->get_popup()->add_item(TTR("Load Default"), ITEM_LOAD_DEFAULT);
        preset->get_popup()->add_separator();
        preset->get_popup()->add_item(FormatSN(TTR("Clear Default for '%s'").asCString(), params->importer->get_visible_name().asCString()), ITEM_CLEAR_DEFAULT);
    }
}

void ImportDock::set_edit_multiple_paths(const Vector<se_string> &p_paths) {

    clear();

    // Use the value that is repeated the most.
    Map<se_string_view, Dictionary> value_frequency;

    for (int i = 0; i < p_paths.size(); i++) {

        Ref<ConfigFile> config(make_ref_counted<ConfigFile>());
        Error err = config->load(p_paths[i] + ".import");
        ERR_CONTINUE(err != OK)

        if (i == 0) {
            params->importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(config->get_value("remap", "importer").as<se_string>());
            if (params->importer==nullptr) {
                clear();
                return;
            }
        }

        PODVector<se_string> keys;
        config->get_section_keys_utf8("params", keys);

        for (const se_string &E : keys) {

            if (!value_frequency.contains(E)) {
                value_frequency[E] = Dictionary();
            }

            Variant value = config->get_value("params", E);

            if (value_frequency[E].has(value)) {
                value_frequency[E][value] = int(value_frequency[E][value]) + 1;
            } else {
                value_frequency[E][value] = 1;
            }
        }
    }

    ERR_FAIL_COND(params->importer==nullptr)

    ListPOD<ResourceImporter::ImportOption> options;
    params->importer->get_import_options(&options);

    params->properties.clear();
    params->values.clear();
    params->checking = true;
    params->checked.clear();

    for (const ResourceImporter::ImportOption &E : options) {

        params->properties.push_back(E.option);

        if (value_frequency.contains(E.option.name.asCString())) {

            Dictionary d = value_frequency[E.option.name.asCString()];
            int freq = 0;
            PODVector<Variant> v(d.get_key_list());
            Variant value;
            for (const Variant &F : v) {
                int f = d[F];
                if (f > freq) {
                    value = F;
                }
            }

            params->values[E.option.name] = value;
        } else {
            params->values[E.option.name] = E.default_value;
        }
    }

    params->update();

    Vector<ResourceImporterInterface * > importers;
    ResourceFormatImporter::get_singleton()->get_importers_for_extension(PathUtils::get_extension(p_paths[0]), &importers);
    ListPOD<Pair<StringName, StringName> > importer_names;

    for (int i=0,fin=importers.size(); i<fin; ++i) {
        importer_names.push_back(Pair<StringName, StringName>(importers[i]->get_visible_name(), importers[i]->get_importer_name()));
    }

    importer_names.sort(PairSort<StringName, StringName>());

    import_as->clear();

    for (const Pair<StringName, StringName> &E : importer_names) {
        import_as->add_item(E.first);
        import_as->set_item_metadata(import_as->get_item_count() - 1, E.second);
        if (E.second == params->importer->get_importer_name()) {
            import_as->select(import_as->get_item_count() - 1);
        }
    }

    preset->get_popup()->clear();

    if (params->importer->get_preset_count() == 0) {
        preset->get_popup()->add_item(TTR("Default"));
    } else {
        for (int i = 0; i < params->importer->get_preset_count(); i++) {
            preset->get_popup()->add_item(params->importer->get_preset_name(i));
        }
    }

    params->paths = p_paths;
    import->set_disabled(false);
    import_as->set_disabled(false);
    preset->set_disabled(false);

    imported->set_text(StringName(itos(p_paths.size()) + TTR(" Files")));
}

void ImportDock::_importer_selected(int i_idx) {
    se_string name = import_as->get_selected_metadata();
    ResourceImporterInterface * importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(name);
    ERR_FAIL_COND(importer==nullptr)

    params->importer = importer;

    Ref<ConfigFile> config;
    if (!params->paths.empty()) {
        config = make_ref_counted<ConfigFile>();
        Error err = config->load(params->paths[0] + ".import");
        if (err != OK) {
            config.unref();
        }
    }
    _update_options(config);
}

void ImportDock::_preset_selected(int p_idx) {

    int item_id = preset->get_popup()->get_item_id(p_idx);
    StringName importer_defaults(se_string("importer_defaults/")+params->importer->get_importer_name());
    switch (item_id) {
        case ITEM_SET_AS_DEFAULT: {
            Dictionary d;

            for (const List<PropertyInfo>::Element *E = params->properties.front(); E; E = E->next()) {
                d[E->deref().name] = params->values[E->deref().name];
            }

            ProjectSettings::get_singleton()->set(importer_defaults, d);
            ProjectSettings::get_singleton()->save();

        } break;
        case ITEM_LOAD_DEFAULT: {

            ERR_FAIL_COND(!ProjectSettings::get_singleton()->has_setting(importer_defaults))

            Dictionary d = ProjectSettings::get_singleton()->get(importer_defaults);
            PODVector<Variant> v(d.get_key_list());

            for (const Variant &E : v) {
                params->values[E] = d[E];
            }
            params->update();

        } break;
        case ITEM_CLEAR_DEFAULT: {

            ProjectSettings::get_singleton()->set(StringName(importer_defaults), Variant());
            ProjectSettings::get_singleton()->save();

        } break;
        default: {

            ListPOD<ResourceImporter::ImportOption> options;

            params->importer->get_import_options(&options, p_idx);

            for (const ResourceImporter::ImportOption &E : options) {

                params->values[E.option.name] = E.default_value;
            }

            params->update();
        } break;
    }
}

void ImportDock::clear() {

    imported->set_text("");
    import->set_disabled(true);
    import_as->clear();
    import_as->set_disabled(true);
    preset->set_disabled(true);
    params->values.clear();
    params->properties.clear();
    params->update();
    preset->get_popup()->clear();
}

static bool _find_owners(EditorFileSystemDirectory *efsd, se_string_view p_path) {

    if (!efsd)
        return false;

    for (int i = 0; i < efsd->get_subdir_count(); i++) {

        if (_find_owners(efsd->get_subdir(i), p_path)) {
            return true;
        }
    }

    for (int i = 0; i < efsd->get_file_count(); i++) {

        Vector<se_string> deps = efsd->get_file_deps(i);
        if (deps.find(se_string(p_path)) != -1)
            return true;
    }

    return false;
}
void ImportDock::_reimport_attempt() {

    bool need_restart = false;
    bool used_in_resources = false;
    for (int i = 0; i < params->paths.size(); i++) {
        Ref<ConfigFile> config(make_ref_counted<ConfigFile>());
        Error err = config->load(params->paths[i] + ".import");
        ERR_CONTINUE(err != OK)

        StringName imported_with(config->get_value("remap", "importer"));
        if (imported_with != params->importer->get_importer_name()) {
            need_restart = true;
            if (_find_owners(EditorFileSystem::get_singleton()->get_filesystem(), params->paths[i])) {
                used_in_resources = true;
            }
        }
    }

    if (need_restart) {
        label_warning->set_visible(used_in_resources);
        reimport_confirm->popup_centered_minsize();
        return;
    }

    _reimport();
}

void ImportDock::_reimport_and_restart() {

    EditorNode::get_singleton()->save_all_scenes();
    EditorResourcePreview::get_singleton()->stop(); //don't try to re-create previews after import
    _reimport();
    EditorNode::get_singleton()->restart_editor();
}

void ImportDock::_reimport() {

    for (int i = 0; i < params->paths.size(); i++) {

        Ref<ConfigFile> config(make_ref_counted<ConfigFile>());
        Error err = config->load(params->paths[i] + ".import");
        ERR_CONTINUE(err != OK);

        StringName importer_name = params->importer->get_importer_name();

        if (params->checking) {
            //update only what edited (checkboxes)
            for (List<PropertyInfo>::Element *E = params->properties.front(); E; E = E->next()) {
                if (params->checked.contains(E->deref().name)) {
                    config->set_value("params", E->deref().name.asCString(), params->values[E->deref().name]);
                }
            }
        } else {
            //override entirely
            config->set_value("remap", "importer", importer_name);
            config->erase_section("params");

            for (List<PropertyInfo>::Element *E = params->properties.front(); E; E = E->next()) {
                config->set_value("params", E->deref().name.asCString(), params->values[E->deref().name]);
            }
        }

        //handle group file
        ResourceImporterInterface *importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(importer_name);
        ERR_CONTINUE(importer==nullptr)
        StringName group_file_property(importer->get_option_group_file());
        if (!group_file_property.empty()) {
            //can import from a group (as in, atlas)
            ERR_CONTINUE(!params->values.contains(group_file_property))
            se_string group_file = params->values[group_file_property];
            config->set_value("remap", "group_file", group_file);
        } else {
            config->set_value("remap", "group_file", Variant()); //clear group file if unused
        }

        config->save(params->paths[i] + ".import");
    }

    EditorFileSystem::get_singleton()->reimport_files(params->paths);
    EditorFileSystem::get_singleton()->emit_signal("filesystem_changed"); //it changed, so force emitting the signal
}

void ImportDock::_notification(int p_what) {
    switch (p_what) {

        case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {

            imported->add_style_override("normal", get_stylebox("normal", "LineEdit"));
        } break;

        case NOTIFICATION_ENTER_TREE: {

            import_opts->edit(params);
            label_warning->add_color_override("font_color", get_color("warning_color", "Editor"));
        } break;
    }
}

void ImportDock::_property_toggled(const StringName &p_prop, bool p_checked) {
    if (p_checked) {
        params->checked.insert(p_prop);
    } else {
        params->checked.erase(p_prop);
    }
}
void ImportDock::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("_reimport"), &ImportDock::_reimport);
    MethodBinder::bind_method(D_METHOD("_preset_selected"), &ImportDock::_preset_selected);
    MethodBinder::bind_method(D_METHOD("_importer_selected"), &ImportDock::_importer_selected);
    MethodBinder::bind_method(D_METHOD("_property_toggled"), &ImportDock::_property_toggled);
    MethodBinder::bind_method(D_METHOD("_reimport_and_restart"), &ImportDock::_reimport_and_restart);
    MethodBinder::bind_method(D_METHOD("_reimport_attempt"), &ImportDock::_reimport_attempt);
}

void ImportDock::initialize_import_options() const {

    ERR_FAIL_COND(!import_opts || !params)

    import_opts->edit(params);
}

ImportDock::ImportDock() {

    set_name("Import");
    imported = memnew(Label);
    imported->add_style_override("normal", EditorNode::get_singleton()->get_gui_base()->get_stylebox("normal", "LineEdit"));
    imported->set_clip_text(true);
    add_child(imported);
    HBoxContainer *hb = memnew(HBoxContainer);
    add_margin_child(TTR("Import As:"), hb);
    import_as = memnew(OptionButton);
    import_as->set_disabled(true);
    import_as->connect("item_selected", this, "_importer_selected");
    hb->add_child(import_as);
    import_as->set_h_size_flags(SIZE_EXPAND_FILL);
    preset = memnew(MenuButton);
    preset->set_text(TTR("Preset"));
    preset->set_disabled(true);
    preset->get_popup()->connect("index_pressed", this, "_preset_selected");
    hb->add_child(preset);

    import_opts = memnew(EditorInspector);
    add_child(import_opts);
    import_opts->set_v_size_flags(SIZE_EXPAND_FILL);
    import_opts->connect("property_toggled", this, "_property_toggled");

    hb = memnew(HBoxContainer);
    add_child(hb);
    import = memnew(Button);
    import->set_text(TTR("Reimport"));
    import->set_disabled(true);
    import->connect("pressed", this, "_reimport_attempt");
    hb->add_spacer();
    hb->add_child(import);
    hb->add_spacer();

    reimport_confirm = memnew(ConfirmationDialog);
    reimport_confirm->get_ok()->set_text(TTR("Save scenes, re-import and restart"));
    add_child(reimport_confirm);
    reimport_confirm->connect("confirmed", this, "_reimport_and_restart");

    VBoxContainer *vbc_confirm = memnew(VBoxContainer());
    vbc_confirm->add_child(memnew(Label(TTR("Changing the type of an imported file requires editor restart."))));
    label_warning = memnew(Label(TTR("WARNING: Assets exist that use this resource, they may stop loading properly.")));
    vbc_confirm->add_child(label_warning);
    reimport_confirm->add_child(vbc_confirm);

    params = memnew(ImportDockParameters);
}

ImportDock::~ImportDock() {

    memdelete(params);
}
