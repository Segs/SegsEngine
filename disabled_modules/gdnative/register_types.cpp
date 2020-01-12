/*************************************************************************/
/*  register_types.cpp                                                   */
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

#include "register_types.h"

#include "gdnative/gdnative.h"

#include "gdnative.h"

#include "arvr/register_types.h"
#include "nativescript/register_types.h"
#include "net/register_types.h"
#include "pluginscript/register_types.h"
#include "videodecoder/register_types.h"

#include "core/class_db.h"
#include "core/engine.h"
#include "core/se_string.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/translation_helpers.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_export.h"
#include "editor/editor_node.h"
#include "gdnative_library_editor_plugin.h"
#include "gdnative_library_singleton_editor.h"
#include <QResource>


class GDNativeExportPlugin : public EditorExportPlugin {

protected:
    void _export_file(const String &p_path, const String &p_type, const Set<String> &p_features) override;
};

struct LibrarySymbol {
    const char *name;
    bool is_required;
};

void GDNativeExportPlugin::_export_file(const String &p_path, const String &p_type, const Set<String> &p_features) {
    if (p_type != "GDNativeLibrary") {
        return;
    }

    Ref<GDNativeLibrary> lib = dynamic_ref_cast<GDNativeLibrary>(ResourceLoader::load(p_path));

    if (not lib) {
        return;
    }
    se_string lib_sym_prefix(StringUtils::to_utf8(lib->get_symbol_prefix()).data());
    Ref<ConfigFile> config = lib->get_config_file();

    {

        PODVector<se_string> entry_keys;
        config->get_section_keys_utf8("entry", entry_keys);

        for (const se_string &key : entry_keys) {

            Vector<String> tags = StringUtils::split(String(key),'.');

            bool skip = false;
            for (int i = 0; i < tags.size(); i++) {
                bool has_feature = p_features.contains(String(tags[i]));

                if (!has_feature) {
                    skip = true;
                    break;
                }
            }

            if (skip) {
                continue;
            }

            String entry_lib_path = config->get_value("entry", key);
            if (!StringUtils::begins_with(entry_lib_path,"res://")) {
                print_line("Skipping export of out-of-project library " + entry_lib_path);
                continue;
            }

            add_shared_object(entry_lib_path, tags);
        }
    }

    {
        PODVector<se_string> dependency_keys;
        config->get_section_keys_utf8("dependencies", dependency_keys);

        for (const se_string &key : dependency_keys) {

            Vector<String> tags = StringUtils::split(String(key),'.');

            bool skip = false;
            for (int i = 0; i < tags.size(); i++) {
                bool has_feature = p_features.contains(tags[i]);

                if (!has_feature) {
                    skip = true;
                    break;
                }
            }

            if (skip) {
                continue;
            }

            Vector<String> dependency_paths = config->get_value("dependencies", key);
            for (int i = 0; i < dependency_paths.size(); i++) {
                if (!StringUtils::begins_with(dependency_paths[i],"res://")) {
                    print_line("Skipping export of out-of-project library " + dependency_paths[i]);
                    continue;
                }
                add_shared_object(dependency_paths[i], tags);
            }
        }
    }
}

static void editor_init_callback() {

    GDNativeLibrarySingletonEditor::initialize_class();
    GDNativeLibraryEditorPlugin::initialize_class();

    GDNativeLibrarySingletonEditor *library_editor = memnew(GDNativeLibrarySingletonEditor);
    library_editor->set_name(TTR("GDNative"));
    ProjectSettingsEditor::get_singleton()->get_tabs()->add_child(library_editor);

    Ref<GDNativeExportPlugin> export_plugin(make_ref_counted<GDNativeExportPlugin>());

    EditorExport::get_singleton()->add_export_plugin(export_plugin);

    EditorNode::get_singleton()->add_editor_plugin(memnew(GDNativeLibraryEditorPlugin(EditorNode::get_singleton())));
}

#endif

static godot_variant cb_standard_varcall(void *p_procedure_handle, godot_array *p_args) {

    godot_gdnative_procedure_fn proc;
    proc = (godot_gdnative_procedure_fn)p_procedure_handle;

    return proc(p_args);
}

GDNativeCallRegistry *GDNativeCallRegistry::singleton;

Vector<Ref<GDNative> > singleton_gdnatives;

Ref<GDNativeLibraryResourceLoader> resource_loader_gdnlib;
Ref<GDNativeLibraryResourceSaver> resource_saver_gdnlib;

void register_gdnative_types() {

#ifdef TOOLS_ENABLED
    Q_INIT_RESOURCE(gdnative);
    GDNativeLibraryEditor::initialize_class();
    EditorNode::add_init_callback(editor_init_callback);
#endif

    ClassDB::register_class<GDNativeLibrary>();
    ClassDB::register_class<GDNative>();

    resource_loader_gdnlib = make_ref_counted<GDNativeLibraryResourceLoader>();
    ResourceLoader::add_resource_format_loader(resource_loader_gdnlib);

    resource_saver_gdnlib = make_ref_counted<GDNativeLibraryResourceSaver>();
    ResourceSaver::add_resource_format_saver(resource_saver_gdnlib);

    GDNativeCallRegistry::singleton = memnew(GDNativeCallRegistry);

    GDNativeCallRegistry::singleton->register_native_call_type("standard_varcall", cb_standard_varcall);

    register_net_types();
    register_arvr_types();
    register_nativescript_types();
    register_pluginscript_types();
    register_videodecoder_types();

    // run singletons

    Array singletons = Array();
    if (ProjectSettings::get_singleton()->has_setting("gdnative/singletons")) {
        singletons = ProjectSettings::get_singleton()->get("gdnative/singletons");
    }
    Array excluded = Array();
    if (ProjectSettings::get_singleton()->has_setting("gdnative/singletons_disabled")) {
        excluded = ProjectSettings::get_singleton()->get("gdnative/singletons_disabled");
    }

    for (int i = 0; i < singletons.size(); i++) {
        String path = singletons[i];

        if (excluded.contains(path))
            continue;

        Ref<GDNativeLibrary> lib = dynamic_ref_cast<GDNativeLibrary>(ResourceLoader::load(path));
        Ref<GDNative> singleton(make_ref_counted<GDNative>());
        singleton->set_library(lib);

        if (!singleton->initialize()) {
            // Can't initialize. Don't make a native_call then
            continue;
        }

        void *proc_ptr;
        Error err = singleton->get_symbol(
                StringName(lib->get_symbol_prefix() + "gdnative_singleton"),
                proc_ptr);

        if (err != OK) {
            ERR_PRINT("No " + lib->get_symbol_prefix() + "gdnative_singleton in \"" + singleton->get_library()->get_current_library_path() + "\" found")
        } else {
            singleton_gdnatives.push_back(singleton);
            ((void (*)())proc_ptr)();
        }
    }
}

void unregister_gdnative_types() {

    for (int i = 0; i < singleton_gdnatives.size(); i++) {

        if (not singleton_gdnatives[i]) {
            continue;
        }

        if (!singleton_gdnatives[i]->is_initialized()) {
            continue;
        }

        singleton_gdnatives.write[i]->terminate();
    }
    singleton_gdnatives.clear();

    unregister_videodecoder_types();
    unregister_pluginscript_types();
    unregister_nativescript_types();
    unregister_arvr_types();
    unregister_net_types();

    memdelete(GDNativeCallRegistry::singleton);

    ResourceLoader::remove_resource_format_loader(resource_loader_gdnlib);
    resource_loader_gdnlib.unref();

    ResourceSaver::remove_resource_format_saver(resource_saver_gdnlib);
    resource_saver_gdnlib.unref();

    // This is for printing out the sizes of the core types

    /*
    print_line(String("array:\t")     + itos(sizeof(Array)));
    print_line(String("basis:\t")     + itos(sizeof(Basis)));
    print_line(String("color:\t")     + itos(sizeof(Color)));
    print_line(String("dict:\t" )     + itos(sizeof(Dictionary)));
    print_line(String("node_path:\t") + itos(sizeof(NodePath)));
    print_line(String("plane:\t")     + itos(sizeof(Plane)));
    print_line(String("poolarray:\t") + itos(sizeof(PoolByteArray)));
    print_line(String("quat:\t")      + itos(sizeof(Quat)));
    print_line(String("rect2:\t")     + itos(sizeof(Rect2)));
    print_line(String("aabb:\t")     + itos(sizeof(AABB)));
    print_line(String("rid:\t")       + itos(sizeof(RID)));
    print_line(String("string:\t")    + itos(sizeof(String)));
    print_line(String("transform:\t") + itos(sizeof(Transform)));
    print_line(String("transfo2D:\t") + itos(sizeof(Transform2D)));
    print_line(String("variant:\t")   + itos(sizeof(Variant)));
    print_line(String("vector2:\t")   + itos(sizeof(Vector2)));
    print_line(String("vector3:\t")   + itos(sizeof(Vector3)));
    */
}
