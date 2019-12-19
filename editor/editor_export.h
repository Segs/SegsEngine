/*************************************************************************/
/*  editor_export.h                                                      */
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

#pragma once

#include "core/list.h"
#include "core/map.h"
#include "core/os/dir_access.h"
#include "core/resource.h"
#include "core/property_info.h"
#include "scene/main/node.h"
#include "scene/main/timer.h"
#include "scene/resources/texture.h"

class FileAccess;
class EditorExportPlatform;
class EditorFileSystemDirectory;
struct EditorProgress;

class EditorExportPreset : public RefCounted {

    GDCLASS(EditorExportPreset,RefCounted)

public:
    enum ExportFilter {
        EXPORT_ALL_RESOURCES,
        EXPORT_SELECTED_SCENES,
        EXPORT_SELECTED_RESOURCES,
    };

    enum ScriptExportMode {
        MODE_SCRIPT_TEXT,
        MODE_SCRIPT_COMPILED,
        MODE_SCRIPT_ENCRYPTED,
    };

private:
    Ref<EditorExportPlatform> platform;
    ExportFilter export_filter;
    se_string include_filter;
    se_string exclude_filter;
    se_string export_path;

    se_string exporter;
    Set<se_string> selected_files;
    bool runnable;

    Vector<se_string> patches;

    friend class EditorExport;
    friend class EditorExportPlatform;

    List<PropertyInfo> properties;
    Map<StringName, Variant> values;

    se_string name;

    se_string custom_features;

    int script_mode;
    se_string script_key;

protected:
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(ListPOD<PropertyInfo> *p_list) const;

public:
    Ref<EditorExportPlatform> get_platform() const;

    bool has(const StringName &p_property) const { return values.contains(p_property); }

    Vector<se_string> get_files_to_export() const;

    void add_export_file(se_string_view p_path);
    void remove_export_file(se_string_view p_path);
    bool has_export_file(se_string_view p_path);

    void set_name(se_string_view p_name);
    const se_string &get_name() const;

    void set_runnable(bool p_enable);
    bool is_runnable() const;

    void set_export_filter(ExportFilter p_filter);
    ExportFilter get_export_filter() const;

    void set_include_filter(se_string_view p_include);
    const se_string &get_include_filter() const;

    void set_exclude_filter(se_string_view p_exclude);
    const se_string &get_exclude_filter() const;

    void add_patch(se_string_view p_path, int p_at_pos = -1);
    void set_patch(int p_index, se_string_view p_path);
    const se_string &get_patch(int p_index);
    void remove_patch(int p_idx);
    Vector<se_string> get_patches() const;

    void set_custom_features(se_string_view p_custom_features);
    const se_string & get_custom_features() const;

    void set_export_path(se_string_view p_path);
    const se_string &get_export_path() const;

    void set_script_export_mode(int p_mode);
    int get_script_export_mode() const;

    void set_script_encryption_key(const se_string &p_key);
    const se_string &get_script_encryption_key() const;

    const List<PropertyInfo> &get_properties() const { return properties; }

    EditorExportPreset();
};

struct SharedObject {
    se_string path;
    Vector<se_string> tags;

    SharedObject(se_string_view p_path, const Vector<se_string> &p_tags) :
            path(p_path),
            tags(p_tags) {
    }

    SharedObject() {}
};

class EditorExportPlatform : public RefCounted {

    GDCLASS(EditorExportPlatform,RefCounted)

public:
    using EditorExportSaveFunction = Error (*)(void *, se_string_view, const Vector<uint8_t> &, int, int);
    using EditorExportSaveSharedObject = Error (*)(void *, const SharedObject &);

private:
    struct FeatureContainers {
        Set<se_string> features;
        PoolVector<se_string> features_pv;
    };

    void _export_find_resources(EditorFileSystemDirectory *p_dir, Set<se_string> &p_paths);
    void _export_find_dependencies(se_string_view p_path, Set<se_string> &p_paths);

    void gen_debug_flags(Vector<se_string> &r_flags, int p_flags);
    static Error _save_pack_file(void *p_userdata, se_string_view p_path, const Vector<uint8_t> &p_data, int p_file, int p_total);
    static Error _save_zip_file(void *p_userdata, se_string_view p_path, const Vector<uint8_t> &p_data, int p_file, int p_total);

    void _edit_files_with_filter(DirAccess *da, const Vector<se_string> &p_filters, Set<se_string> &r_list, bool exclude);
    void _edit_filter_list(Set<se_string> &r_list, se_string_view p_filter, bool exclude);

    static Error _add_shared_object(void *p_userdata, const SharedObject &p_so);

protected:
    struct ExportNotifier {
        ExportNotifier(EditorExportPlatform &p_platform, const Ref<EditorExportPreset> &p_preset, bool p_debug, se_string_view p_path, int p_flags);
        ~ExportNotifier();
    };

    FeatureContainers get_feature_containers(const Ref<EditorExportPreset> &p_preset);

    bool exists_export_template(se_string_view template_file_name, se_string *err) const;
    se_string find_export_template(se_string_view template_file_name, se_string *err = nullptr) const;
    void gen_export_flags(Vector<se_string> &r_flags, int p_flags);

public:
    virtual void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<se_string> *r_features) = 0;

    struct ExportOption {
        PropertyInfo option;
        Variant default_value;

        ExportOption(const PropertyInfo &p_info, const Variant &p_default) :
                option(p_info),
                default_value(p_default) {
        }
        ExportOption() {}
    };

    virtual Ref<EditorExportPreset> create_preset();

    virtual void get_export_options(List<ExportOption> *r_options) = 0;
    virtual bool get_option_visibility(const StringName &p_option, const Map<StringName, Variant> &p_options) const { return true; }

    virtual const se_string & get_os_name() const = 0;
    virtual const se_string & get_name() const = 0;
    virtual Ref<Texture> get_logo() const = 0;

    Error export_project_files(const Ref<EditorExportPreset> &p_preset, EditorExportSaveFunction p_func, void *p_udata, EditorExportSaveSharedObject p_so_func = nullptr);

    Error save_pack(const Ref<EditorExportPreset> &p_preset, se_string_view p_path, Vector<SharedObject> *p_so_files = nullptr, bool p_embed = false, int64_t *r_embedded_start = nullptr, int64_t *r_embedded_size = nullptr);
    Error save_zip(const Ref<EditorExportPreset> &p_preset, const se_string &p_path);

    virtual bool poll_export() { return false; }
    virtual int get_options_count() const { return 0; }
    virtual const se_string & get_options_tooltip() const { return null_se_string; }
    virtual Ref<ImageTexture> get_option_icon(int p_index) const;
    virtual StringName get_option_label(int p_device) const { return StringName(); }
    virtual StringName get_option_tooltip(int p_device) const { return StringName(); }

    enum DebugFlags {
        DEBUG_FLAG_DUMB_CLIENT = 1,
        DEBUG_FLAG_REMOTE_DEBUG = 2,
        DEBUG_FLAG_REMOTE_DEBUG_LOCALHOST = 4,
        DEBUG_FLAG_VIEW_COLLISONS = 8,
        DEBUG_FLAG_VIEW_NAVIGATION = 16,
    };

    virtual Error run(const Ref<EditorExportPreset> &p_preset, int p_device, int p_debug_flags) { return OK; }
    virtual Ref<Texture> get_run_icon() const { return get_logo(); }

    StringName test_etc2() const; //generic test for etc2 since most platforms use it
    virtual bool can_export(const Ref<EditorExportPreset> &p_preset, se_string &r_error, bool &r_missing_templates) const = 0;

    virtual List<se_string> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const = 0;
    virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, se_string_view p_path, int p_flags = 0) = 0;
    virtual Error export_pack(const Ref<EditorExportPreset> &p_preset, bool p_debug, se_string_view p_path, int p_flags = 0);
    virtual Error export_zip(const Ref<EditorExportPreset> &p_preset, bool p_debug, se_string_view p_path, int p_flags = 0);
    virtual void get_platform_features(List<se_string> *r_features) = 0;
    virtual void resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, Set<se_string> &p_features) = 0;

    EditorExportPlatform();
};

class EditorExportPlugin : public RefCounted {
    GDCLASS(EditorExportPlugin,RefCounted)

    friend class EditorExportPlatform;

    Ref<EditorExportPreset> export_preset;

    Vector<SharedObject> shared_objects;
    struct ExtraFile {
        se_string path;
        Vector<uint8_t> data;
        bool remap;
    };
    Vector<ExtraFile> extra_files;
    bool skipped;

    _FORCE_INLINE_ void _clear() {
        shared_objects.clear();
        extra_files.clear();
        skipped = false;
    }

    _FORCE_INLINE_ void _export_end() {
    }

    void _export_file_script(se_string_view p_path, se_string_view p_type, const PoolVector<se_string> &p_features);
    void _export_begin_script(const PoolVector<se_string> &p_features, bool p_debug, se_string_view p_path, int p_flags);
    void _export_end_script();

protected:
    void set_export_preset(const Ref<EditorExportPreset> &p_preset);
    Ref<EditorExportPreset> get_export_preset() const;

    void add_file(se_string_view p_path, const Vector<uint8_t> &p_file, bool p_remap);
    void add_shared_object(se_string_view p_path, const Vector<se_string> &tags);

    void skip();

    virtual void _export_file(se_string_view p_path, se_string_view p_type, const Set<se_string> &p_features);
    virtual void _export_begin(const Set<se_string> &p_features, bool p_debug, se_string_view p_path, int p_flags);

    static void _bind_methods();

public:

    EditorExportPlugin();
};

class EditorExport : public Node {
    GDCLASS(EditorExport,Node)

    Vector<Ref<EditorExportPlatform> > export_platforms;
    Vector<Ref<EditorExportPreset> > export_presets;
    Vector<Ref<EditorExportPlugin> > export_plugins;

    Timer *save_timer;
    bool block_save;

    static EditorExport *singleton;

    void _save();

protected:
    friend class EditorExportPreset;
    void save_presets();

    void _notification(int p_what);
    static void _bind_methods();

public:
    static EditorExport *get_singleton() { return singleton; }

    void add_export_platform(const Ref<EditorExportPlatform> &p_platform);
    int get_export_platform_count();
    Ref<EditorExportPlatform> get_export_platform(int p_idx);

    void add_export_preset(const Ref<EditorExportPreset> &p_preset, int p_at_pos = -1);
    int get_export_preset_count() const;
    Ref<EditorExportPreset> get_export_preset(int p_idx);
    void remove_export_preset(int p_idx);

    void add_export_plugin(const Ref<EditorExportPlugin> &p_plugin);
    void remove_export_plugin(const Ref<EditorExportPlugin> &p_plugin);
    Vector<Ref<EditorExportPlugin> > get_export_plugins();

    void load_config();

    bool poll_export_platforms();

    EditorExport();
    ~EditorExport() override;
};

class EditorExportPlatformPC : public EditorExportPlatform {

    GDCLASS(EditorExportPlatformPC,EditorExportPlatform)

public:
    using FixUpEmbeddedPckFunc = Error (*)(se_string_view, int64_t, int64_t);

private:
    Ref<ImageTexture> logo;
    se_string name;
    se_string os_name;
    Map<se_string, se_string> extensions;

    se_string release_file_32;
    se_string release_file_64;
    se_string debug_file_32;
    se_string debug_file_64;

    Set<se_string> extra_features;

    int chmod_flags;

    FixUpEmbeddedPckFunc fixup_embedded_pck_func;

public:
    void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<se_string> *r_features) override;

    void get_export_options(List<ExportOption> *r_options) override;

    const se_string &get_name() const override;
    const se_string &get_os_name() const override;
    Ref<Texture> get_logo() const override;

    bool can_export(const Ref<EditorExportPreset> &p_preset, se_string &r_error, bool &r_missing_templates) const override;
    List<se_string> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
    Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, se_string_view p_path, int p_flags = 0) override;
    virtual Error sign_shared_object(const Ref<EditorExportPreset> &p_preset, bool p_debug, se_string_view p_path);

    void set_extension(se_string_view p_extension, se_string_view p_feature_key = "default");
    void set_name(se_string_view p_name);
    void set_os_name(se_string_view p_name);

    void set_logo(const Ref<Texture> &p_logo);

    void set_release_64(se_string_view p_file);
    void set_release_32(se_string_view p_file);
    void set_debug_64(se_string_view p_file);
    void set_debug_32(se_string_view p_file);

    void add_platform_feature(se_string_view p_feature);
    void get_platform_features(List<se_string> *r_features) override;
    void resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, Set<se_string> &p_features) override;

    int get_chmod_flags() const;
    void set_chmod_flags(int p_flags);

    FixUpEmbeddedPckFunc get_fixup_embedded_pck_func() const;
    void set_fixup_embedded_pck_func(FixUpEmbeddedPckFunc p_fixup_embedded_pck_func);

    EditorExportPlatformPC();
};

class EditorExportTextSceneToBinaryPlugin : public EditorExportPlugin {

    GDCLASS(EditorExportTextSceneToBinaryPlugin,EditorExportPlugin)

public:
    void _export_file(se_string_view p_path, se_string_view p_type, const Set<se_string> &p_features) override;
    EditorExportTextSceneToBinaryPlugin();
};
