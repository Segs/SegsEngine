/*************************************************************************/
/*  editor_export.h                                                      */
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

#include "core/list.h"
#include "core/set.h"
#include "core/map.h"
#include "core/os/dir_access.h"
#include "core/resource.h"
#include "core/property_info.h"
#include "scene/main/node.h"
#include "scene/main/timer.h"
#include "scene/resources/texture.h"
class RichTextLabel;
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
    ExportFilter export_filter = EXPORT_ALL_RESOURCES;
    String include_filter;
    String exclude_filter;
    String export_path;
    String exporter;
    String name;
    String custom_features;
    String script_key;

    Set<String> selected_files;
    Vector<String> patches;
    Vector<PropertyInfo> properties;
    HashMap<StringName, Variant> values;
    HashMap<StringName, bool> update_visibility;

    int script_mode = MODE_SCRIPT_COMPILED;
    bool runnable = false;

    friend class EditorExport;
    friend class EditorExportPlatform;
protected:
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(Vector<PropertyInfo> *p_list) const;

public:
    Ref<EditorExportPlatform> get_platform() const;

    bool has(const StringName &p_property) const { return values.contains(p_property); }

    Vector<String> get_files_to_export() const;

    void add_export_file(StringView p_path);
    void remove_export_file(StringView p_path);
    bool has_export_file(StringView p_path);

    void set_name(StringView p_name);
    const String &get_name() const;

    void set_runnable(bool p_enable);
    bool is_runnable() const;

    void set_export_filter(ExportFilter p_filter);
    ExportFilter get_export_filter() const;

    void set_include_filter(StringView p_include);
    const String &get_include_filter() const;

    void set_exclude_filter(StringView p_exclude);
    const String &get_exclude_filter() const;

    void add_patch(StringView p_path, int p_at_pos = -1);
    void set_patch(int p_index, StringView p_path);
    const String &get_patch(int p_index);
    void remove_patch(int p_idx);
    const Vector<String> &get_patches() const { return patches; }

    void set_custom_features(StringView p_custom_features);
    const String & get_custom_features() const;

    void set_export_path(StringView p_path);
    const String &get_export_path() const;

    void set_script_export_mode(int p_mode);
    int get_script_export_mode() const;

    void set_script_encryption_key(const String &p_key);
    const String &get_script_encryption_key() const;

    const Vector<PropertyInfo> &get_properties() const { return properties; }

    EditorExportPreset();
};

struct SharedObject {
    String path;
    Vector<String> tags;

    SharedObject(StringView p_path, const Vector<String> &p_tags) :
            path(p_path),
            tags(p_tags) {
    }

    SharedObject() {}
};

class GODOT_EXPORT EditorExportPlatform : public RefCounted {

    GDCLASS(EditorExportPlatform,RefCounted)

public:
    using EditorExportSaveFunction = Error (*)(void *, StringView, const Vector<uint8_t> &, int, int);
    using EditorExportSaveSharedObject = Error (*)(void *, const SharedObject &);
    enum ExportMessageType {
        EXPORT_MESSAGE_NONE,
        EXPORT_MESSAGE_INFO,
        EXPORT_MESSAGE_WARNING,
        EXPORT_MESSAGE_ERROR,
    };

    struct ExportMessage {
        ExportMessageType msg_type;
        String category;
        String text;
    };

private:
    Ref<ImageTexture> logo;
    String name;
    String os_name;
    Map<String, String> extensions;

    String release_file_32;
    String release_file_64;
    String debug_file_32;
    String debug_file_64;

    Set<String> extra_features;

    int chmod_flags = -1;
    struct FeatureContainers {
        Set<String> features;
        PoolVector<String> features_pv;
    };
    Vector<ExportMessage> messages;

    void _export_find_resources(EditorFileSystemDirectory *p_dir, Set<String> &p_paths);
    void _export_find_dependencies(StringView p_path, Set<String> &p_paths);

    void gen_debug_flags(Vector<String> &r_flags, int p_flags);
    static Error _save_pack_file(void *p_userdata, StringView p_path, const Vector<uint8_t> &p_data, int p_file, int p_total);
    static Error _save_zip_file(void *p_userdata, StringView p_path, const Vector<uint8_t> &p_data, int p_file, int p_total);

    void _edit_files_with_filter(DirAccess *da, const Vector<String> &p_filters, Set<String> &r_list, bool exclude);
    void _edit_filter_list(Set<String> &r_list, StringView p_filter, bool exclude);

    static Error _add_shared_object(void *p_userdata, const SharedObject &p_so);

protected:
    struct GODOT_EXPORT ExportNotifier {
        ExportNotifier(EditorExportPlatform &p_platform, const Ref<EditorExportPreset> &p_preset, bool p_debug, StringView p_path, int p_flags);
        ~ExportNotifier();
    };

    FeatureContainers get_feature_containers(const Ref<EditorExportPreset> &p_preset);

    bool exists_export_template(StringView template_file_name, String *err) const;
    String find_export_template(StringView template_file_name, String *err = nullptr) const;
    void gen_export_flags(Vector<String> &r_flags, int p_flags);

public:

    struct ExportOption {
        PropertyInfo option;
        Variant default_value;
        bool update_visibility = false;

        ExportOption(const PropertyInfo& p_info, const Variant& p_default, bool p_update_visibility = false) :
            option(p_info),
            default_value(p_default),
            update_visibility(p_update_visibility) {
        }
        ExportOption() {}
    };

    virtual Ref<EditorExportPreset> create_preset();

    virtual void clear_messages() { messages.clear(); }
    virtual void add_message(ExportMessageType p_type, const String& p_category, const String& p_message);
    void add_message(ExportMessageType p_type, const StringName& p_category, const StringName& p_message)
    {
        add_message(p_type, String(p_category.asCString()), String(p_message.asCString()));
    }

    virtual int get_message_count() const {
        return messages.size();
    }

    virtual ExportMessage get_message(int p_index) const {
        ERR_FAIL_INDEX_V(p_index, messages.size(), ExportMessage());
        return messages[p_index];
    }

    virtual ExportMessageType get_worst_message_type() const {
        ExportMessageType worst_type = EXPORT_MESSAGE_NONE;
        for (const auto& message : messages) {
            worst_type = M_MAX(worst_type, message.msg_type);
        }
        return worst_type;
    }

    bool fill_log_messages(RichTextLabel* p_log, Error p_err);

    virtual bool should_update_export_options() { return false; }
    virtual bool get_option_visibility(const EditorExportPreset* p_preset, const StringName &p_option, const HashMap<StringName, Variant> &p_options) const { return true; }


    Error export_project_files(const Ref<EditorExportPreset> &p_preset, EditorExportSaveFunction p_func, void *p_udata, EditorExportSaveSharedObject p_so_func = nullptr);

    Error save_pack(const Ref<EditorExportPreset> &p_preset, StringView p_path, Vector<SharedObject> *p_so_files = nullptr, bool p_embed = false, int64_t *r_embedded_start = nullptr, int64_t *r_embedded_size = nullptr);
    Error save_zip(const Ref<EditorExportPreset> &p_preset, const String &p_path);


    enum DebugFlags {
        DEBUG_FLAG_DUMB_CLIENT = 1,
        DEBUG_FLAG_REMOTE_DEBUG = 2,
        DEBUG_FLAG_REMOTE_DEBUG_LOCALHOST = 4,
        DEBUG_FLAG_VIEW_COLLISONS = 8,
        DEBUG_FLAG_VIEW_NAVIGATION = 16,
        DEBUG_FLAG_SHADER_FALLBACKS = 32,
    };

    virtual Error run(const Ref<EditorExportPreset> &p_preset, int p_device, int p_debug_flags) { return OK; }
    virtual Ref<Texture> get_run_icon() const { return get_logo(); }

    bool can_export(const Ref<EditorExportPreset>& p_preset, String& r_error, bool& r_missing_templates) const;
    Error export_pack(const Ref<EditorExportPreset>& p_preset, bool p_debug, StringView p_path, int p_flags = 0);
    Error export_zip(const Ref<EditorExportPreset>& p_preset, bool p_debug, StringView p_path, int p_flags = 0);

    EditorExportPlatform();

public:
    virtual void get_preset_features(const Ref<EditorExportPreset>& p_preset, Vector<String>* r_features);

    virtual void get_export_options(Vector<ExportOption>* r_options);

    const String& get_name() const;
    const String& get_os_name() const;
    const Ref<ImageTexture>& get_logo() const { return logo; }

    virtual bool has_valid_export_configuration(const Ref<EditorExportPreset>& p_preset, String& r_error, bool& r_missing_templates) const;
    virtual bool has_valid_project_configuration(const Ref<EditorExportPreset>& p_preset, String& r_error) const;

    virtual Vector<String> get_binary_extensions(const Ref<EditorExportPreset>& p_preset) const;
    virtual Error export_project(const Ref<EditorExportPreset>& p_preset, bool p_debug, StringView p_path, int p_flags = 0);

    virtual Error sign_shared_object(const Ref<EditorExportPreset>& p_preset, bool p_debug, StringView p_path);
    virtual Error prepare_template(const Ref<EditorExportPreset>& p_preset, bool p_debug, StringView p_path, int p_flags);
    virtual Error modify_template(const Ref<EditorExportPreset>& p_preset, bool p_debug, StringView p_path, int p_flags) { return OK; }
    virtual Error export_project_data(const Ref<EditorExportPreset>& p_preset, bool p_debug, StringView p_path, int p_flags);
    virtual Error fixup_embedded_pck(StringView p_path, int64_t p_embedded_start, int64_t p_embedded_size) { return OK; }


    void set_extension(StringView p_extension, StringView p_feature_key = "default");
    void set_name(StringView p_name);
    void set_os_name(StringView p_name);

    void set_logo(const Ref<Texture>& p_logo);

    void set_release_64(StringView p_file);
    void set_release_32(StringView p_file);
    void set_debug_64(StringView p_file);
    void set_debug_32(StringView p_file);

    void add_platform_feature(StringView p_feature);
    virtual void get_platform_features(Vector<String>* r_features);
    virtual void resolve_platform_feature_priorities(const Ref<EditorExportPreset>& p_preset, Set<String>& p_features);

    int get_chmod_flags() const;
    void set_chmod_flags(int p_flags);
};

class GODOT_EXPORT EditorExportPlugin : public RefCounted {
    GDCLASS(EditorExportPlugin,RefCounted)

    friend class EditorExportPlatform;

    Ref<EditorExportPreset> export_preset;

    Vector<SharedObject> shared_objects;
    struct ExtraFile {
        String path;
        Vector<uint8_t> data;
        bool remap;
    };
    Vector<ExtraFile> extra_files;
    Vector<String> osx_plugin_files;
    bool skipped;

    _FORCE_INLINE_ void _clear() {
        shared_objects.clear();
        extra_files.clear();
        osx_plugin_files.clear();
        skipped = false;
    }

    _FORCE_INLINE_ void _export_end() {
    }

    void _export_file_script(StringView p_path, StringView p_type, const PoolVector<String> &p_features);
    void _export_begin_script(const PoolVector<String> &p_features, bool p_debug, StringView p_path, int p_flags);
    void _export_end_script();

protected:
    void set_export_preset(const Ref<EditorExportPreset> &p_preset);
    Ref<EditorExportPreset> get_export_preset() const;
public: // exposed to scripting
    void add_file(StringView p_path, const Vector<uint8_t> &p_file, bool p_remap);
    void add_shared_object(StringView p_path, const Vector<String> &tags);
    void skip();
    void add_osx_plugin_file(const String &p_path) {
        osx_plugin_files.push_back(p_path);
    }
protected:

    virtual void _export_file(StringView p_path, StringView p_type, const Set<String> &p_features);
    virtual void _export_begin(const Set<String> &p_features, bool p_debug, StringView p_path, int p_flags);

    static void _bind_methods();

public:
    const Vector<String> &get_osx_plugin_files() const { return osx_plugin_files; }

    EditorExportPlugin();
};

class GODOT_EXPORT EditorExport : public Node {
    GDCLASS(EditorExport,Node)

    Vector<Ref<EditorExportPlatform> > export_platforms;
    Vector<Ref<EditorExportPreset> > export_presets;
    Vector<Ref<EditorExportPlugin> > export_plugins;

    StringName _export_presets_updated;

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
    void remove_export_plugin_by_impl(const Ref<EditorExportPlugin>& p_plugin);
    const Vector<Ref<EditorExportPlugin> > &get_export_plugins();

    void load_config();
    void update_export_presets();

    EditorExport();
    ~EditorExport() override;
};


class EditorExportTextSceneToBinaryPlugin : public EditorExportPlugin {

    GDCLASS(EditorExportTextSceneToBinaryPlugin,EditorExportPlugin)

public:
    void _export_file(StringView p_path, StringView p_type, const Set<String> &p_features) override;
    EditorExportTextSceneToBinaryPlugin();
};
