/*************************************************************************/
/*  editor_export.cpp                                                    */
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

#include "editor_export.h"

#include "core/callable_method_pointer.h"
#include "core/crypto/crypto_core.h"
#include "core/io/config_file.h"
#include "core/io/file_access_pack.h" // PACK_HEADER_MAGIC, PACK_FORMAT_VERSION
#include "core/io/zip_io.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/project_settings.h"
#include "core/resource/resource_manager.h"
#include "core/script_language.h"
#include "core/version.h"
#include "editor/editor_file_system.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor_node.h"
#include "editor_scale.h"
#include "editor_settings.h"
#include "core/string_formatter.h"
#include "scene/resources/resource_format_text.h"
#include "scene/resources/theme.h"

#include "EASTL/sort.h"
#include "scene/gui/rich_text_label.h"

IMPL_GDCLASS(EditorExportPreset)
IMPL_GDCLASS(EditorExportPlatform)
IMPL_GDCLASS(EditorExportPlugin)
IMPL_GDCLASS(EditorExport)
IMPL_GDCLASS(EditorExportTextSceneToBinaryPlugin)
namespace {

struct SavedData {
    uint64_t ofs;
    uint64_t size;
    Vector<uint8_t> md5;
    String path_utf8;

    bool operator<(const SavedData &p_data) const { return path_utf8 < p_data.path_utf8; }
};

struct PackData {
    FileAccess *f;
    Vector<SavedData> file_ofs;
    EditorProgress *ep;
    Vector<SharedObject> *so_files;
};

struct ZipData {
    void *zip;
    EditorProgress *ep;
};

int _get_pad(int p_alignment, int p_n) {
    int rest = p_n % p_alignment;
    int pad = 0;
    if (rest > 0) {
        pad = p_alignment - rest;
    }

    return pad;
}
} // end of anonymous namespace

#define PCK_PADDING 16

bool EditorExportPreset::_set(const StringName &p_name, const Variant &p_value) {
    if (values.contains(p_name)) {
        values[p_name] = p_value;
        EditorExport::singleton->save_presets();
        if (update_visibility[p_name]) {
            property_list_changed_notify();
        }
        return true;
    }

    return false;
}

bool EditorExportPreset::_get(const StringName &p_name, Variant &r_ret) const {
    if (values.contains(p_name)) {
        r_ret = values.at(p_name);
        return true;
    }

    return false;
}

void EditorExportPreset::_get_property_list(Vector<PropertyInfo> *p_list) const {
    for (const PropertyInfo &E : properties) {
        if (platform->get_option_visibility(this,E.name, values)) {
            p_list->push_back(E);
        }
    }
}

Ref<EditorExportPlatform> EditorExportPreset::get_platform() const {
    return platform;
}

Vector<String> EditorExportPreset::get_files_to_export() const {
    Vector<String> files;
    for (const String &E : selected_files) {
        files.emplace_back(E);
    }
    return files;
}

void EditorExportPreset::set_name(StringView p_name) {
    name = p_name;
    EditorExport::singleton->save_presets();
}

const String &EditorExportPreset::get_name() const {
    return name;
}

void EditorExportPreset::set_runnable(bool p_enable) {
    runnable = p_enable;
    EditorExport::singleton->save_presets();
}

bool EditorExportPreset::is_runnable() const {
    return runnable;
}

void EditorExportPreset::set_export_filter(ExportFilter p_filter) {
    export_filter = p_filter;
    EditorExport::singleton->save_presets();
}

EditorExportPreset::ExportFilter EditorExportPreset::get_export_filter() const {
    return export_filter;
}

void EditorExportPreset::set_include_filter(StringView p_include) {
    include_filter = p_include;
    EditorExport::singleton->save_presets();
}

const String &EditorExportPreset::get_include_filter() const {
    return include_filter;
}

void EditorExportPreset::set_export_path(StringView p_path) {
    export_path = p_path;
    /* NOTE(SonerSound): if there is a need to implement a PropertyHint that specifically indicates a relative path,
     * this should be removed. */
    if (PathUtils::is_abs_path(export_path)) {
        String res_path = OS::get_singleton()->get_resource_dir();
        export_path = PathUtils::path_to_file(res_path, export_path);
    }
    EditorExport::singleton->save_presets();
}

const String &EditorExportPreset::get_export_path() const {
    return export_path;
}

void EditorExportPreset::set_exclude_filter(StringView p_exclude) {
    exclude_filter = p_exclude;
    EditorExport::singleton->save_presets();
}

const String &EditorExportPreset::get_exclude_filter() const {
    return exclude_filter;
}

void EditorExportPreset::add_export_file(StringView p_path) {
    selected_files.insert(p_path);
    EditorExport::singleton->save_presets();
}

void EditorExportPreset::remove_export_file(StringView p_path) {
    auto iter = selected_files.find_as(p_path);
    if (iter != selected_files.end()) {
        selected_files.erase(iter);
    }
    EditorExport::singleton->save_presets();
}

bool EditorExportPreset::has_export_file(StringView p_path) {
    return selected_files.contains_as(p_path);
}

void EditorExportPreset::add_patch(StringView p_path, int p_at_pos) {
    if (p_at_pos < 0) {
        patches.emplace_back(p_path);
    } else {
        patches.insert_at(p_at_pos, String(p_path));
    }
    EditorExport::singleton->save_presets();
}

void EditorExportPreset::remove_patch(int p_idx) {
    ERR_FAIL_INDEX(p_idx, patches.size());
    patches.erase_at(p_idx);
    EditorExport::singleton->save_presets();
}

void EditorExportPreset::set_patch(int p_index, StringView p_path) {
    ERR_FAIL_INDEX(p_index, patches.size());
    patches[p_index] = p_path;
    EditorExport::singleton->save_presets();
}
const String &EditorExportPreset::get_patch(int p_index) {
    ERR_FAIL_INDEX_V(p_index, patches.size(), null_string);
    return patches[p_index];
}

void EditorExportPreset::set_custom_features(StringView p_custom_features) {
    custom_features = p_custom_features;
    EditorExport::singleton->save_presets();
}

const String &EditorExportPreset::get_custom_features() const {
    return custom_features;
}

void EditorExportPreset::set_script_export_mode(int p_mode) {
    script_mode = p_mode;
    EditorExport::singleton->save_presets();
}

int EditorExportPreset::get_script_export_mode() const {
    return script_mode;
}

void EditorExportPreset::set_script_encryption_key(const String &p_key) {
    script_key = p_key;
    EditorExport::singleton->save_presets();
}

const String &EditorExportPreset::get_script_encryption_key() const {
    return script_key;
}

EditorExportPreset::EditorExportPreset() {}

///////////////////////////////////
bool EditorExportPlatform::fill_log_messages(RichTextLabel* p_log, Error p_err) {
    bool has_messages = false;

    int msg_count = get_message_count();

    p_log->add_text(TTR("Project export for platform:") + " ");
    p_log->add_image(get_logo(), 16 * EDSCALE, 16 * EDSCALE, RichTextLabel::INLINE_ALIGN_CENTER);
    p_log->add_text(" ");
    p_log->add_text(get_name());
    p_log->add_text(" - ");
    if (p_err == OK) {
        if (get_worst_message_type() >= EditorExportPlatform::EXPORT_MESSAGE_WARNING) {
            p_log->add_image(EditorNode::get_singleton()->get_gui_base()->get_theme_icon("StatusWarning", "EditorIcons"), 16 * EDSCALE, 16 * EDSCALE, RichTextLabel::INLINE_ALIGN_CENTER);
            p_log->add_text(" ");
            p_log->add_text(TTR("Completed with warnings."));
            has_messages = true;
        }
        else {
            p_log->add_image(EditorNode::get_singleton()->get_gui_base()->get_theme_icon("StatusSuccess", "EditorIcons"), 16 * EDSCALE, 16 * EDSCALE, RichTextLabel::INLINE_ALIGN_CENTER);
            p_log->add_text(" ");
            p_log->add_text(TTR("Completed successfully."));
            if (msg_count > 0) {
                has_messages = true;
            }
        }
    }
    else {
        p_log->add_image(EditorNode::get_singleton()->get_gui_base()->get_theme_icon("StatusError", "EditorIcons"), 16 * EDSCALE, 16 * EDSCALE, RichTextLabel::INLINE_ALIGN_CENTER);
        p_log->add_text(" ");
        p_log->add_text(TTR("Failed."));
        has_messages = true;
    }

    if (msg_count) {
        p_log->push_table(2);
        p_log->set_table_column_expand(0, false);
        p_log->set_table_column_expand(1, true);
        for (int m = 0; m < msg_count; m++) {
            EditorExportPlatform::ExportMessage msg = get_message(m);
            Color color = EditorNode::get_singleton()->get_gui_base()->get_theme_color("font_color", "Label");
            Ref<Texture> icon;

            switch (msg.msg_type) {
            case EditorExportPlatform::EXPORT_MESSAGE_INFO: {
                color = EditorNode::get_singleton()->get_gui_base()->get_theme_color("font_color", "Editor") * Color(1, 1, 1, 0.6);
            } break;
            case EditorExportPlatform::EXPORT_MESSAGE_WARNING: {
                icon = EditorNode::get_singleton()->get_gui_base()->get_theme_icon("Warning", "EditorIcons");
                color = EditorNode::get_singleton()->get_gui_base()->get_theme_color("warning_color", "Editor");
            } break;
            case EditorExportPlatform::EXPORT_MESSAGE_ERROR: {
                icon = EditorNode::get_singleton()->get_gui_base()->get_theme_icon("Error", "EditorIcons");
                color = EditorNode::get_singleton()->get_gui_base()->get_theme_color("error_color", "Editor");
            } break;
            default:
                break;
            }

            p_log->push_cell();
            p_log->add_text("\t");
            if (icon) {
                p_log->add_image(icon);
            }
            p_log->pop();

            p_log->push_cell();
            p_log->push_color(color);
            p_log->add_text(FormatVE("[%s]: %s", msg.category.c_str(), msg.text.c_str()));
            p_log->pop();
            p_log->pop();
        }
        p_log->pop();
        p_log->add_newline();
    }
    p_log->add_newline();
    return has_messages;
}

void EditorExportPlatform::gen_debug_flags(Vector<String> &r_flags, int p_flags) {
    String host = EditorSettings::get_singleton()->getT<String>("network/debug/remote_host");
    int remote_port = EditorSettings::get_singleton()->getT<int>("network/debug/remote_port");

    if (p_flags & DEBUG_FLAG_REMOTE_DEBUG_LOCALHOST) {
        host = "localhost";
    }

    if (p_flags & DEBUG_FLAG_DUMB_CLIENT) {
        int port = EditorSettings::get_singleton()->getT<int>("filesystem/file_server/port");
        String passwd = EditorSettings::get_singleton()->getT<String>("filesystem/file_server/password");
        r_flags.emplace_back("--remote-fs");
        r_flags.emplace_back(host + ":" + itos(port));
        if (!passwd.empty()) {
            r_flags.emplace_back("--remote-fs-password");
            r_flags.emplace_back(passwd);
        }
    }

    if (p_flags & DEBUG_FLAG_REMOTE_DEBUG) {
        r_flags.emplace_back("--remote-debug");

        r_flags.emplace_back(host + ":" + StringUtils::num(remote_port));

        Vector<String> breakpoints;
        ScriptEditor::get_singleton()->get_breakpoints(&breakpoints);

        if (!breakpoints.empty()) {
            String bpoints = String::joined(breakpoints, ",").replaced(" ", "%20");
            r_flags.emplace_back("--breakpoints");
            r_flags.emplace_back(eastl::move(bpoints));
        }
    }

    if (p_flags & DEBUG_FLAG_VIEW_COLLISONS) {
        r_flags.emplace_back("--debug-collisions");
    }

    if (p_flags & DEBUG_FLAG_VIEW_NAVIGATION) {
        r_flags.emplace_back("--debug_navigation");
    }
    if (p_flags & DEBUG_FLAG_SHADER_FALLBACKS) {
        r_flags.emplace_back("--debug-shader-fallbacks");
    }
}

Error EditorExportPlatform::_save_pack_file(
        void *p_userdata, StringView p_path, const Vector<uint8_t> &p_data, int p_file, int p_total) {
    PackData *pd = (PackData *)p_userdata;

    SavedData sd;
    sd.path_utf8 = p_path;
    sd.ofs = pd->f->get_position();
    sd.size = p_data.size();

    pd->f->store_buffer(p_data.data(), p_data.size());
    int pad = _get_pad(PCK_PADDING, sd.size);
    for (int i = 0; i < pad; i++) {
        pd->f->store_8(0);
    }

    {
        unsigned char hash[16];
        CryptoCore::md5(p_data.data(), p_data.size(), hash);
        sd.md5.resize(16);
        for (int i = 0; i < 16; i++) {
            sd.md5[i] = hash[i];
        }
    }

    pd->file_ofs.push_back(sd);

    if (pd->ep->step(TTR("Storing File:") + " " + p_path, 2 + p_file * 100 / p_total, false)) {
        return ERR_SKIP;
    }

    return OK;
}

Error EditorExportPlatform::_save_zip_file(
        void *p_userdata, StringView p_path, const Vector<uint8_t> &p_data, int p_file, int p_total) {
    String path = StringUtils::replace_first(p_path, "res://", String());

    ZipData *zd = (ZipData *)p_userdata;

    zipFile zip = zd->zip;

    zipOpenNewFileInZip(zip, path.c_str(), nullptr, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, Z_DEFAULT_COMPRESSION);

    zipWriteInFileInZip(zip, p_data.data(), p_data.size());
    zipCloseFileInZip(zip);

    if (zd->ep->step(TTR("Storing File:") + " " + p_path, 2 + p_file * 100 / p_total, false)) {
        return ERR_SKIP;
    }

    return OK;
}


String EditorExportPlatform::find_export_template(StringView template_file_name, String *err) const {
    String current_version(VERSION_FULL_CONFIG);
    String template_path = PathUtils::plus_file(
            PathUtils::plus_file(EditorSettings::get_singleton()->get_templates_dir(), current_version),
            template_file_name);

    if (FileAccess::exists(template_path)) {
        return template_path;
    }

    // Not found
    if (err) {
        *err += TTR("No export template found at the expected path:") + "\n" + template_path + "\n";
    }
    return String();
}

bool EditorExportPlatform::exists_export_template(StringView template_file_name, String *err) const {
    return !find_export_template(template_file_name, err).empty();
}

Ref<EditorExportPreset> EditorExportPlatform::create_preset() {
    Ref<EditorExportPreset> preset(make_ref_counted<EditorExportPreset>());
    preset->platform = Ref<EditorExportPlatform>(this);

    Vector<ExportOption> options;
    get_export_options(&options);

    for (const ExportOption &E : options) {
        preset->properties.push_back(E.option);
        preset->values[E.option.name] = E.default_value;
        preset->update_visibility[E.option.name] = E.update_visibility;
    }

    return preset;
}

void EditorExportPlatform::add_message(ExportMessageType p_type, const String& p_category, const String& p_message)
{
    ExportMessage msg;
    msg.category = p_category;
    msg.text = p_message;
    msg.msg_type = p_type;
    messages.push_back(msg);
    switch (p_type) {
    case EXPORT_MESSAGE_INFO: {
        print_line(FormatVE("%s: %s", msg.category.c_str(), msg.text.c_str()));
    } break;
    case EXPORT_MESSAGE_WARNING: {
        WARN_PRINT(FormatVE("%s: %s", msg.category.c_str(), msg.text.c_str()));
    } break;
    case EXPORT_MESSAGE_ERROR: {
        ERR_PRINT(FormatVE("%s: %s", msg.category.c_str(), msg.text.c_str()));
    } break;
    default:
        break;
    }
}

void EditorExportPlatform::_export_find_resources(EditorFileSystemDirectory *p_dir, Set<String> &p_paths) {
    for (int i = 0; i < p_dir->get_subdir_count(); i++) {
        _export_find_resources(p_dir->get_subdir(i), p_paths);
    }

    for (int i = 0; i < p_dir->get_file_count(); i++) {
        p_paths.insert(p_dir->get_file_path(i));
    }
}

void EditorExportPlatform::_export_find_dependencies(StringView p_path, Set<String> &p_paths) {
    if (p_paths.contains_as(p_path)) {
        return;
    }

    p_paths.insert(p_path);

    EditorFileSystemDirectory *dir;
    int file_idx;
    dir = EditorFileSystem::get_singleton()->find_file(p_path, &file_idx);
    if (!dir) {
        return;
    }

    const Vector<String> &deps = dir->get_file_deps(file_idx);

    for (int i = 0; i < deps.size(); i++) {
        _export_find_dependencies(deps[i], p_paths);
    }
}

void EditorExportPlatform::_edit_files_with_filter(
        DirAccess *da, const Vector<String> &p_filters, Set<String> &r_list, bool exclude) {
    da->list_dir_begin();
    String cur_dir = StringUtils::replace(da->get_current_dir(), "\\", "/");
    if (!StringUtils::ends_with(cur_dir, "/")) {
        cur_dir += '/';
    }
    String cur_dir_no_prefix = StringUtils::replace(cur_dir, "res://", "");

    Vector<String> dirs;
    String f;
    while (!(f = da->get_next()).empty()) {
        if (da->current_is_dir()) {
            dirs.push_back(f);
        } else {
            String fullpath = cur_dir + f;
            // Test also against path without res:// so that filters like `file.txt` can work.
            String fullpath_no_prefix = cur_dir_no_prefix + f;
            for (const String &filter : p_filters) {
                if (StringUtils::match(fullpath, filter, StringUtils::CaseInsensitive) ||
                        StringUtils::match(fullpath_no_prefix, filter, StringUtils::CaseInsensitive)) {
                    if (!exclude) {
                        r_list.insert(fullpath);
                    } else {
                        r_list.erase(fullpath);
                    }
                }
            }
        }
    }

    da->list_dir_end();

    for (int i = 0; i < dirs.size(); ++i) {
        String dir(dirs[i]);
        if (StringUtils::begins_with(dir, ".")) {
            continue;
        }
        if (editor_should_skip_directory(cur_dir + dir))
            continue;
        da->change_dir(dir);
        _edit_files_with_filter(da, p_filters, r_list, exclude);
        da->change_dir("..");
    }
}

void EditorExportPlatform::_edit_filter_list(Set<String> &r_list, StringView p_filter, bool exclude) {
    if (p_filter.empty()) {
        return;
    }
    Vector<StringView> split = StringUtils::split(p_filter, ',');
    Vector<String> filters;
    for (int i = 0; i < split.size(); i++) {
        StringView f = StringUtils::strip_edges(split[i]);
        if (f.empty()) {
            continue;
        }
        filters.emplace_back(f);
    }

    DirAccess *da = DirAccess::open("res://");
    ERR_FAIL_NULL(da);
    _edit_files_with_filter(da, filters, r_list, exclude);
    memdelete(da);
}

void EditorExportPlugin::set_export_preset(const Ref<EditorExportPreset> &p_preset) {
    if (p_preset) {
        export_preset = p_preset;
    }
}

Ref<EditorExportPreset> EditorExportPlugin::get_export_preset() const {
    return export_preset;
}

void EditorExportPlugin::add_file(StringView p_path, const Vector<uint8_t> &p_file, bool p_remap) {
    ExtraFile ef;
    ef.data = p_file;
    ef.path = p_path;
    ef.remap = p_remap;
    extra_files.push_back(ef);
}

void EditorExportPlugin::add_shared_object(StringView p_path, const Vector<String> &tags) {
    shared_objects.push_back(SharedObject(p_path, tags));
}

void EditorExportPlugin::_export_file_script(
        StringView p_path, StringView p_type, const PoolVector<String> &p_features) {
    if (get_script_instance()) {
        get_script_instance()->call("_export_file", p_path, p_type, p_features);
    }
}

void EditorExportPlugin::_export_begin_script(
        const PoolVector<String> &p_features, bool p_debug, StringView p_path, int p_flags) {
    if (get_script_instance()) {
        get_script_instance()->call("_export_begin", p_features, p_debug, p_path, p_flags);
    }
}

void EditorExportPlugin::_export_end_script() {
    if (get_script_instance()) {
        get_script_instance()->call("_export_end");
    }
}

void EditorExportPlugin::_export_file(StringView p_path, StringView p_type, const Set<String> &p_features) {}

void EditorExportPlugin::_export_begin(const Set<String> &p_features, bool p_debug, StringView p_path, int p_flags) {}

void EditorExportPlugin::skip() {
    skipped = true;
}

void EditorExportPlugin::_bind_methods() {
    MethodBinder::bind_method(
            D_METHOD("add_shared_object", { "path", "tags" }), &EditorExportPlugin::add_shared_object);
    SE_BIND_METHOD(EditorExportPlugin,add_file);
    SE_BIND_METHOD(EditorExportPlugin,skip);
    SE_BIND_METHOD(EditorExportPlugin,add_osx_plugin_file);

    BIND_VMETHOD(MethodInfo("_export_file", PropertyInfo(VariantType::STRING, "path"),
            PropertyInfo(VariantType::STRING, "type"), PropertyInfo(VariantType::POOL_STRING_ARRAY, "features")));
    BIND_VMETHOD(MethodInfo("_export_begin", PropertyInfo(VariantType::POOL_STRING_ARRAY, "features"),
            PropertyInfo(VariantType::BOOL, "is_debug"), PropertyInfo(VariantType::STRING, "path"),
            PropertyInfo(VariantType::INT, "flags")));
    BIND_VMETHOD(MethodInfo("_export_end"));
}

EditorExportPlugin::EditorExportPlugin() {
    skipped = false;
}

EditorExportPlatform::FeatureContainers EditorExportPlatform::get_feature_containers(
        const Ref<EditorExportPreset> &p_preset) {
    Ref<EditorExportPlatform> platform = p_preset->get_platform();
    Vector<String> feature_list;
    platform->get_platform_features(&feature_list);
    platform->get_preset_features(p_preset, &feature_list);

    FeatureContainers result;
    for (const String &E : feature_list) {
        result.features.insert(E);
        result.features_pv.push_back(E);
    }

    if (!p_preset->get_custom_features().empty()) {
        Vector<StringView> tmp_custom_list = StringUtils::split(p_preset->get_custom_features(), ',');

        for (size_t i = 0; i < tmp_custom_list.size(); i++) {
            StringView f = StringUtils::strip_edges(tmp_custom_list[i]);
            if (!f.empty()) {
                result.features.emplace(f);
                result.features_pv.push_back(String(f));
            }
        }
    }

    return result;
}

EditorExportPlatform::ExportNotifier::ExportNotifier(EditorExportPlatform &p_platform,
        const Ref<EditorExportPreset> &p_preset, bool p_debug, StringView p_path, int p_flags) {
    FeatureContainers features = p_platform.get_feature_containers(p_preset);
    const Vector<Ref<EditorExportPlugin>> &export_plugins = EditorExport::get_singleton()->get_export_plugins();
    // initial export plugin callback
    for (int i = 0; i < export_plugins.size(); i++) {
        if (export_plugins[i]->get_script_instance()) { // script based
            export_plugins[i]->_export_begin_script(features.features_pv, p_debug, p_path, p_flags);
        } else {
            export_plugins[i]->_export_begin(features.features, p_debug, p_path, p_flags);
        }
    }
}

EditorExportPlatform::ExportNotifier::~ExportNotifier() {
    const Vector<Ref<EditorExportPlugin>> &export_plugins = EditorExport::get_singleton()->get_export_plugins();
    for (int i = 0; i < export_plugins.size(); i++) {
        if (export_plugins[i]->get_script_instance()) {
            export_plugins[i]->_export_end_script();
        }
        export_plugins[i]->_export_end();
    }
}

Error EditorExportPlatform::export_project_files(const Ref<EditorExportPreset> &p_preset,
        EditorExportSaveFunction p_func, void *p_udata, EditorExportSaveSharedObject p_so_func) {
    // figure out paths of files that will be exported
    Set<String> paths;
    PoolVector<String> path_remaps;

    if (p_preset->get_export_filter() == EditorExportPreset::EXPORT_ALL_RESOURCES) {
        // find stuff
        _export_find_resources(EditorFileSystem::get_singleton()->get_filesystem(), paths);
    } else {
        bool scenes_only = p_preset->get_export_filter() == EditorExportPreset::EXPORT_SELECTED_SCENES;

        Vector<String> files = p_preset->get_files_to_export();
        for (int i = 0; i < files.size(); i++) {
            if (scenes_only && gResourceManager().get_resource_type(files[i]) != "PackedScene") {
                continue;
            }

            _export_find_dependencies(files[i], paths);
        }
        // Add autoload resources and their dependencies
        Vector<PropertyInfo> props;
        ProjectSettings::get_singleton()->get_property_list(&props);

        for (const PropertyInfo &pi :props) {
            if (!StringView(pi.name).starts_with("autoload/")) {
                continue;
            }

            String autoload_path = ProjectSettings::get_singleton()->getT<String>(pi.name);

            if (autoload_path.starts_with("*")) {
                autoload_path = autoload_path.substr(1);
            }

            _export_find_dependencies(autoload_path, paths);
        }
    }

    // add native icons to non-resource include list
    _edit_filter_list(paths, "*.icns", false);
    _edit_filter_list(paths, "*.ico", false);

    _edit_filter_list(paths, p_preset->get_include_filter(), false);
    _edit_filter_list(paths, p_preset->get_exclude_filter(), true);

    // Ignore import files, since these are automatically added to the jar later with the resources
    _edit_filter_list(paths, String("*.import"), true);

    const Vector<Ref<EditorExportPlugin>> &export_plugins = EditorExport::get_singleton()->get_export_plugins();
    for (int i = 0; i < export_plugins.size(); i++) {
        export_plugins[i]->set_export_preset(p_preset);

        if (p_so_func) {
            for (int j = 0; j < export_plugins[i]->shared_objects.size(); j++) {
                p_so_func(p_udata, export_plugins[i]->shared_objects[j]);
            }
        }
        for (int j = 0; j < export_plugins[i]->extra_files.size(); j++) {
            p_func(p_udata, export_plugins[i]->extra_files[j].path, export_plugins[i]->extra_files[j].data, 0,
                    paths.size());
        }

        export_plugins[i]->_clear();
    }

    FeatureContainers feature_containers = get_feature_containers(p_preset);
    Set<String> &features = feature_containers.features;
    PoolVector<String> &features_pv = feature_containers.features_pv;

    // store everything in the export medium
    int idx = 0;
    int total = paths.size();

    for (const String &path : paths) {
        String type = gResourceManager().get_resource_type(path);

        if (FileAccess::exists(path + ".import")) {
            // file is imported, replace by what it imports
            Ref<ConfigFile> config(make_ref_counted<ConfigFile>());
            Error err = config->load(path + ".import");
            if (err != OK) {
                ERR_PRINT("Could not parse: '" + path + "', not exported.");
                continue;
            }
            String importer_type = config->get_value("remap", "importer").as<String>();

            if (importer_type == "keep") {
                //just keep file as-is
                Vector<uint8_t> array = FileAccess::get_file_as_array(path);
                err = p_func(p_udata, path, array, idx, total);

                if (err != OK) {
                    return err;
                }

                continue;
            }

            Vector<String> remaps = config->get_section_keys("remap");

            Set<String> remap_features;

            for (const String &remap : remaps) {
                StringView feature = StringUtils::get_slice(remap, ".", 1);
                if (features.contains_as(feature)) {
                    remap_features.insert(feature);
                }
            }

            if (remap_features.size() > 1) {
                this->resolve_platform_feature_priorities(p_preset, remap_features);
            }

            err = OK;

            for (const String &remap : remaps) {
                if (remap == "path") {
                    String remapped_path = config->get_value("remap", "path").as<String>();
                    Vector<uint8_t> array = FileAccess::get_file_as_array(remapped_path);
                    err = p_func(p_udata, remapped_path, array, idx, total);
                } else if (StringUtils::begins_with(remap, "path.")) {
                    StringView feature = StringUtils::get_slice(remap, ".", 1);

                    if (remap_features.contains_as(feature)) {
                        String remapped_path = config->get_value("remap", remap).as<String>();
                        Vector<uint8_t> array = FileAccess::get_file_as_array(remapped_path);
                        err = p_func(p_udata, remapped_path, array, idx, total);
                    }
                }
            }

            if (err != OK) {
                return err;
            }

            // also save the .import file
            Vector<uint8_t> array = FileAccess::get_file_as_array(path + ".import");
            err = p_func(p_udata, path + ".import", array, idx, total);

            if (err != OK) {
                return err;
            }

        } else {
            bool do_export = true;
            for (int i = 0; i < export_plugins.size(); i++) {
                if (export_plugins[i]->get_script_instance()) { // script based
                    export_plugins[i]->_export_file_script(path, type, features_pv);
                } else {
                    export_plugins[i]->_export_file(path, type, features);
                }
                if (p_so_func) {
                    for (int j = 0; j < export_plugins[i]->shared_objects.size(); j++) {
                        p_so_func(p_udata, export_plugins[i]->shared_objects[j]);
                    }
                }

                for (int j = 0; j < export_plugins[i]->extra_files.size(); j++) {
                    p_func(p_udata, export_plugins[i]->extra_files[j].path, export_plugins[i]->extra_files[j].data, idx,
                            total);
                    if (export_plugins[i]->extra_files[j].remap) {
                        do_export = false; // if remap, do not
                        path_remaps.push_back(path);
                        path_remaps.push_back(export_plugins[i]->extra_files[j].path);
                    }
                }

                if (export_plugins[i]->skipped) {
                    do_export = false;
                }
                export_plugins[i]->_clear();

                if (!do_export) {
                    break; // apologies, not exporting
                }
            }
            // just store it as it comes
            if (do_export) {
                Vector<uint8_t> array = FileAccess::get_file_as_array(path);
                p_func(p_udata, path, array, idx, total);
            }
        }

        idx++;
    }

    // save config!

    Vector<String> custom_list;

    if (!p_preset->get_custom_features().empty()) {
        Vector<StringView> tmp_custom_list = StringUtils::split(p_preset->get_custom_features(), ',');

        for (int i = 0; i < tmp_custom_list.size(); i++) {
            StringView f = StringUtils::strip_edges(tmp_custom_list[i]);
            if (!f.empty()) {
                custom_list.emplace_back(f);
            }
        }
    }

    ProjectSettings::CustomMap custom_map;
    if (!path_remaps.empty()) {
        for (int i = 0; i < path_remaps.size(); i += 2) {
            const String &from = path_remaps[i];
            const String &to = path_remaps[i + 1];
            String remap_file = "[remap]\n\npath=\"" + StringUtils::c_escape(to) + "\"\n";
            Vector<uint8_t> new_file;
            new_file.resize(remap_file.length());
            for (size_t j = 0; j < remap_file.length(); j++) {
                new_file[j] = remap_file[j];
            }

            p_func(p_udata, from + ".remap", new_file, idx, total);
        }
    }

    // Store icon and splash images directly, they need to bypass the import system and be loaded as images
    auto icon = ProjectSettings::get_singleton()->getT<String>("application/config/icon");
    auto splash = ProjectSettings::get_singleton()->getT<String>("application/boot_splash/image");
    if (!icon.empty() && FileAccess::exists(icon)) {
        Vector<uint8_t> array = FileAccess::get_file_as_array(icon);
        p_func(p_udata, icon, array, idx, total);
    }
    if (!splash.empty() && FileAccess::exists(splash) && icon != splash) {
        Vector<uint8_t> array = FileAccess::get_file_as_array(splash);
        p_func(p_udata, splash, array, idx, total);
    }

    String config_file("project.binary");
    String engine_cfb = PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(), "tmp" + config_file);
    ProjectSettings::get_singleton()->save_custom(engine_cfb, custom_map, custom_list);
    Vector<uint8_t> data = FileAccess::get_file_as_array(engine_cfb);
    DirAccess::remove_file_or_error(engine_cfb);

    p_func(p_udata, "res://" + config_file, data, idx, total);

    return OK;
}

Error EditorExportPlatform::_add_shared_object(void *p_userdata, const SharedObject &p_so) {
    PackData *pack_data = (PackData *)p_userdata;
    if (pack_data->so_files) {
        pack_data->so_files->push_back(p_so);
    }

    return OK;
}

Error EditorExportPlatform::save_pack(const Ref<EditorExportPreset> &p_preset, StringView p_path,
        Vector<SharedObject> *p_so_files, bool p_embed, int64_t *r_embedded_start, int64_t *r_embedded_size) {
    EditorProgress ep(("savepack"), TTR("Packing"), 102, true);

    // Create the temporary export directory if it doesn't exist.
    DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    da->make_dir_recursive(EditorSettings::get_singleton()->get_cache_dir());
    String tmppath = PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(), "packtmp");
    FileAccess *ftmp = FileAccess::open(tmppath, FileAccess::WRITE);
    if (!ftmp) {
        add_message(EXPORT_MESSAGE_ERROR, TTR("Save PCK").asCString(), FormatVE(TTR("Cannot create file \"%s\".").asCString(), tmppath.c_str()));
        return ERR_CANT_CREATE;
    }
    PackData pd;
    pd.ep = &ep;
    pd.f = ftmp;
    pd.so_files = p_so_files;

    Error err = export_project_files(p_preset, _save_pack_file, &pd, _add_shared_object);

    memdelete(ftmp); // close tmp file

    if (err != OK) {
        DirAccess::remove_file_or_error(tmppath);
        add_message(EXPORT_MESSAGE_ERROR, TTR("Save PCK"), TTR("Failed to export project files."));

        return err;
    }

    eastl::sort(pd.file_ofs.begin(), pd.file_ofs.end()); // do sort, so we can do binary search later

    FileAccess *f;
    int64_t embed_pos = 0;
    if (!p_embed) {
        // Regular output to separate PCK file
        f = FileAccess::open(p_path, FileAccess::WRITE);
        if (!f) {
            DirAccess::remove_file_or_error(tmppath);
            ERR_FAIL_V(ERR_CANT_CREATE);
        }
    } else {
        // Append to executable
        f = FileAccess::open(p_path, FileAccess::READ_WRITE);
        if (!f) {
            DirAccess::remove_file_or_error(tmppath);
            ERR_FAIL_V(ERR_FILE_CANT_OPEN);
        }

        f->seek_end();
        embed_pos = f->get_position();

        if (r_embedded_start) {
            *r_embedded_start = embed_pos;
        }

        // Ensure embedded PCK starts at a 64-bit multiple
        int pad = f->get_position() % 8;
        for (int i = 0; i < pad; i++) {
            f->store_8(0);
        }
    }

    int64_t pck_start_pos = f->get_position();

    f->store_32(PACK_HEADER_MAGIC); // GDPC
    f->store_32(PACK_FORMAT_VERSION); // pack version
    f->store_32(VERSION_MAJOR);
    f->store_32(VERSION_MINOR);
    f->store_32(VERSION_PATCH);
    for (int i = 0; i < 16; i++) {
        // reserved
        f->store_32(0);
    }

    f->store_32(pd.file_ofs.size()); // amount of files

    int64_t header_size = f->get_position();

    // precalculate header size

    for (int i = 0; i < pd.file_ofs.size(); i++) {
        header_size += 4; // size of path string (32 bits is enough)
        int string_len = pd.file_ofs[i].path_utf8.length();
        header_size += string_len + _get_pad(4, string_len); /// size of path string
        header_size += 8; // offset to file _with_ header size included
        header_size += 8; // size of file
        header_size += 16; // md5
    }

    int header_padding = _get_pad(PCK_PADDING, header_size);

    for (int i = 0; i < pd.file_ofs.size(); i++) {
        uint32_t string_len = pd.file_ofs[i].path_utf8.length();
        uint32_t pad = _get_pad(4, string_len);

        f->store_32(string_len + pad);
        f->store_buffer((const uint8_t *)pd.file_ofs[i].path_utf8.data(), string_len);
        for (uint32_t j = 0; j < pad; j++) {
            f->store_8(0);
        }

        f->store_64(pd.file_ofs[i].ofs + header_padding + header_size);
        f->store_64(pd.file_ofs[i].size); // pay attention here, this is where file is
        f->store_buffer(pd.file_ofs[i].md5.data(), 16); // also save md5 for file
    }

    for (int i = 0; i < header_padding; i++) {
        f->store_8(0);
    }

    // Save the rest of the data.

    ftmp = FileAccess::open(tmppath, FileAccess::READ);
    if (!ftmp) {
        memdelete(f);
        DirAccess::remove_file_or_error(tmppath);
        add_message(EXPORT_MESSAGE_ERROR, TTR("Save PCK").asCString(), FormatVE(TTR("Can't open file to read from path \"%s\".").asCString(), tmppath.c_str()));
        return ERR_CANT_CREATE;
    }

    const int bufsize = 16384;
    uint8_t buf[bufsize];

    while (true) {
        uint64_t got = ftmp->get_buffer(buf, bufsize);
        if (got == 0) {
            break;
        }
        f->store_buffer(buf, got);
    }

    memdelete(ftmp);

    if (p_embed) {
        // Ensure embedded data ends at a 64-bit multiple
        uint64_t embed_end = f->get_position() - embed_pos + 12;
        uint64_t pad = embed_end % 8;
        for (uint64_t i = 0; i < pad; i++) {
            f->store_8(0);
        }

        uint64_t pck_size = f->get_position() - pck_start_pos;
        f->store_64(pck_size);
        f->store_32(PACK_HEADER_MAGIC);

        if (r_embedded_size) {
            *r_embedded_size = f->get_position() - embed_pos;
        }
    }

    memdelete(f);
    DirAccess::remove_file_or_error(tmppath);

    return OK;
}

Error EditorExportPlatform::save_zip(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
    EditorProgress ep(("savezip"), TTR("Packing"), 102, true);

    FileAccess *src_f;
    zlib_filefunc_def io = zipio_create_io_from_file(&src_f);
    zipFile zip = zipOpen2(p_path.c_str(), APPEND_STATUS_CREATE, nullptr, &io);

    ZipData zd;
    zd.ep = &ep;
    zd.zip = zip;

    Error err = export_project_files(p_preset, _save_zip_file, &zd);
    if (err != OK && err != ERR_SKIP) {
        add_message(EXPORT_MESSAGE_ERROR, TTR("Save ZIP"), TTR("Failed to export project files."));
    }

    zipClose(zip, nullptr);

    return OK;
}

Error EditorExportPlatform::export_pack(
        const Ref<EditorExportPreset> &p_preset, bool p_debug, StringView p_path, int p_flags) {
    ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);
    return save_pack(p_preset, p_path);
}

Error EditorExportPlatform::export_zip(
        const Ref<EditorExportPreset> &p_preset, bool p_debug, StringView p_path, int p_flags) {
    ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);
    return save_zip(p_preset, String(p_path));
}

void EditorExportPlatform::gen_export_flags(Vector<String> &r_flags, int p_flags) {
    auto host = EditorSettings::get_singleton()->getT<String>("network/debug/remote_host");
    int remote_port = EditorSettings::get_singleton()->getT<int>("network/debug/remote_port");

    if (p_flags & DEBUG_FLAG_REMOTE_DEBUG_LOCALHOST) {
        host = "localhost";
    }

    if (p_flags & DEBUG_FLAG_DUMB_CLIENT) {
        int port = EditorSettings::get_singleton()->getT<int>("filesystem/file_server/port");
        auto passwd = EditorSettings::get_singleton()->getT<String>("filesystem/file_server/password");
        r_flags.push_back("--remote-fs");
        r_flags.push_back(host + ":" + itos(port));
        if (!passwd.empty()) {
            r_flags.push_back("--remote-fs-password");
            r_flags.push_back(passwd);
        }
    }

    if (p_flags & DEBUG_FLAG_REMOTE_DEBUG) {
        r_flags.push_back("--remote-debug");

        r_flags.push_back(host + ":" + StringUtils::num(remote_port));

        Vector<String> breakpoints;
        ScriptEditor::get_singleton()->get_breakpoints(&breakpoints);

        if (!breakpoints.empty()) {
            String bpoints = String::joined(breakpoints, ",").replaced(" ", "%20");

            r_flags.emplace_back("--breakpoints");
            r_flags.emplace_back(bpoints);
        }
    }

    if (p_flags & DEBUG_FLAG_VIEW_COLLISONS) {
        r_flags.push_back("--debug-collisions");
    }

    if (p_flags & DEBUG_FLAG_VIEW_NAVIGATION) {
        r_flags.push_back("--debug_navigation");
    }
}
EditorExportPlatform::EditorExportPlatform() {}

////

EditorExport *EditorExport::singleton = nullptr;

void EditorExport::_save() {
    Ref<ConfigFile> config(make_ref_counted<ConfigFile>());
    for (int i = 0; i < export_presets.size(); i++) {
        Ref<EditorExportPreset> preset = export_presets[i];
        String section = "preset." + ::to_string(i);

        config->set_value(section, "name", preset->get_name());
        config->set_value(section, "platform", preset->get_platform()->get_name());
        config->set_value(section, "runnable", preset->is_runnable());
        config->set_value(section, "custom_features", preset->get_custom_features());

        bool save_files = false;
        switch (preset->get_export_filter()) {
            case EditorExportPreset::EXPORT_ALL_RESOURCES: {
                config->set_value(section, "export_filter", "all_resources");
            } break;
            case EditorExportPreset::EXPORT_SELECTED_SCENES: {
                config->set_value(section, "export_filter", "scenes");
                save_files = true;
            } break;
            case EditorExportPreset::EXPORT_SELECTED_RESOURCES: {
                config->set_value(section, "export_filter", "resources");
                save_files = true;
            } break;
        }

        if (save_files) {
            Vector<String> export_files = preset->get_files_to_export();
            config->set_value(section, "export_files", Variant::from(eastl::move(export_files)));
        }
        config->set_value(section, "include_filter", preset->get_include_filter());
        config->set_value(section, "exclude_filter", preset->get_exclude_filter());
        config->set_value(section, "export_path", preset->get_export_path());
        config->set_value(section, "patch_list", Variant::from(preset->get_patches()));
        config->set_value(section, "script_export_mode", preset->get_script_export_mode());
        config->set_value(section, "script_encryption_key", Variant(preset->get_script_encryption_key()));

        String option_section = "preset." + ::to_string(i) + ".options";

        for (const PropertyInfo &E : preset->get_properties()) {
            config->set_value(option_section, E.name, preset->get(E.name));
        }
    }

    config->save("res://export_presets.cfg");
}

void EditorExport::save_presets() {
    if (block_save) {
        return;
    }
    save_timer->start();
}

void EditorExport::_bind_methods() {
    ADD_SIGNAL(MethodInfo("export_presets_updated"));
}

void EditorExport::add_export_platform(const Ref<EditorExportPlatform> &p_platform) {
    export_platforms.push_back(p_platform);
}

int EditorExport::get_export_platform_count() {
    return export_platforms.size();
}

Ref<EditorExportPlatform> EditorExport::get_export_platform(int p_idx) {
    ERR_FAIL_INDEX_V(p_idx, export_platforms.size(), Ref<EditorExportPlatform>());

    return export_platforms[p_idx];
}

void EditorExport::add_export_preset(const Ref<EditorExportPreset> &p_preset, int p_at_pos) {
    if (p_at_pos < 0) {
        export_presets.push_back(p_preset);
    } else {
        export_presets.insert_at(p_at_pos, p_preset);
    }
}

int EditorExport::get_export_preset_count() const {
    return export_presets.size();
}

Ref<EditorExportPreset> EditorExport::get_export_preset(int p_idx) {
    ERR_FAIL_INDEX_V(p_idx, export_presets.size(), Ref<EditorExportPreset>());
    return export_presets[p_idx];
}

void EditorExport::remove_export_preset(int p_idx) {
    export_presets.erase_at(p_idx);
    save_presets();
}

void EditorExport::add_export_plugin(const Ref<EditorExportPlugin> &p_plugin) {
    if (not export_plugins.contains(p_plugin)) {
        export_plugins.push_back(p_plugin);
    }
}

void EditorExport::remove_export_plugin(const Ref<EditorExportPlugin> &p_plugin) {
    export_plugins.erase_first(p_plugin);
}

const Vector<Ref<EditorExportPlugin>> &EditorExport::get_export_plugins() {
    return export_plugins;
}

void EditorExport::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            load_config();
        } break;
        case NOTIFICATION_PROCESS: {
            update_export_presets();
        } break;
    }
}

void EditorExport::load_config() {
    Ref<ConfigFile> config(make_ref_counted<ConfigFile>());
    Error err = config->load("res://export_presets.cfg");
    if (err != OK) {
        return;
    }

    block_save = true;

    int index = 0;
    while (true) {
        String section = "preset." + ::to_string(index);
        if (!config->has_section(section)) {
            break;
        }

        const String platform = config->get_value(section, "platform").as<String>();

        Ref<EditorExportPreset> preset;

        for (int i = 0; i < export_platforms.size(); i++) {
            if (export_platforms[i]->get_name() == platform) {
                preset = export_platforms[i]->create_preset();
                break;
            }
        }

        if (not preset) {
            index++;
            ERR_CONTINUE(not preset);
        }

        preset->set_name(config->get_value(section, "name").as<String>());
        preset->set_runnable(config->get_value(section, "runnable").as<bool>());

        if (config->has_section_key(section, "custom_features")) {
            preset->set_custom_features(config->get_value(section, "custom_features").as<String>());
        }

        UIString export_filter = config->get_value(section, "export_filter").as<UIString>();

        bool get_files = false;

        if (export_filter == "all_resources") {
            preset->set_export_filter(EditorExportPreset::EXPORT_ALL_RESOURCES);
        } else if (export_filter == "scenes") {
            preset->set_export_filter(EditorExportPreset::EXPORT_SELECTED_SCENES);
            get_files = true;
        } else if (export_filter == "resources") {
            preset->set_export_filter(EditorExportPreset::EXPORT_SELECTED_RESOURCES);
            get_files = true;
        }

        if (get_files) {
            PoolVector<String> files(config->get_value(section, "export_files").as<PoolVector<String>>());

            for (int i = 0; i < files.size(); i++) {
                preset->add_export_file(files[i]);
            }
        }

        preset->set_include_filter(config->get_value(section, "include_filter").as<String>());
        preset->set_exclude_filter(config->get_value(section, "exclude_filter").as<String>());
        preset->set_export_path(config->get_value(section, "export_path", "").as<String>());

        PoolVector<String> patch_list(config->get_value(section, "patch_list").as<PoolVector<String>>());

        for (int i = 0; i < patch_list.size(); i++) {
            preset->add_patch(patch_list[i]);
        }

        if (config->has_section_key(section, "script_export_mode")) {
            preset->set_script_export_mode(config->get_value(section, "script_export_mode").as<int>());
        }
        if (config->has_section_key(section, "script_encryption_key")) {
            preset->set_script_encryption_key(config->get_value(section, "script_encryption_key").as<String>());
        }

        String option_section = "preset." + ::to_string(index) + ".options";

        Vector<String> options = config->get_section_keys(option_section);

        for (const String &E : options) {
            Variant value = config->get_value(option_section, E);

            preset->set(StringName(E), value);
        }

        add_export_preset(preset);
        index++;
    }

    block_save = false;
}
void EditorExport::update_export_presets() {
    Map<StringView, Vector<EditorExportPlatform::ExportOption>> platform_options;

    for (int i = 0; i < export_platforms.size(); i++) {
        Ref<EditorExportPlatform> platform = export_platforms[i];

        if (platform->should_update_export_options()) {
            Vector<EditorExportPlatform::ExportOption> options;
            platform->get_export_options(&options);

            platform_options[platform->get_name()] = options;
        }
    }

    bool export_presets_updated = false;
    for (int i = 0; i < export_presets.size(); i++) {
        Ref<EditorExportPreset> preset = export_presets[i];
        if (platform_options.contains(preset->get_platform()->get_name())) {
            export_presets_updated = true;

            Vector<EditorExportPlatform::ExportOption> &options = platform_options[preset->get_platform()->get_name()];

            // Copy the previous preset values
            const HashMap<StringName, Variant> &previous_values = preset->values;

            // Clear the preset properties and values prior to reloading
            preset->properties.clear();
            preset->values.clear();
            preset->update_visibility.clear();

            for (const EditorExportPlatform::ExportOption &E : options) {
                preset->properties.push_back(E.option);

                StringName option_name = E.option.name;
                preset->values[option_name] =
                        previous_values.contains(option_name) ? previous_values.at(option_name) : E.default_value;
                preset->update_visibility[option_name] = E.update_visibility;
            }
        }
    }

    if (export_presets_updated) {
        emit_signal(_export_presets_updated);
    }
}

EditorExport::EditorExport() {
    save_timer = memnew(Timer);
    add_child(save_timer);
    save_timer->set_wait_time(0.8f);
    save_timer->set_one_shot(true);
    save_timer->connect("timeout", callable_mp(this, &ClassName::_save));
    block_save = false;

    _export_presets_updated = "export_presets_updated";

    singleton = this;
    set_process(true);
}

EditorExport::~EditorExport() {}

//////////

void EditorExportPlatform::get_preset_features(const Ref<EditorExportPreset> &p_preset, Vector<String> *r_features) {
    if (p_preset->get("texture_format/s3tc").as<bool>()) {
        r_features->push_back("s3tc");
    }
    if (p_preset->get("texture_format/bptc").as<bool>()) {
        r_features->push_back("bptc");
    }

    if (p_preset->get("binary_format/64_bits").as<bool>()) {
        r_features->push_back("64");
    } else {
        r_features->push_back("32");
    }
}

void EditorExportPlatform::get_export_options(Vector<EditorExportPlatform::ExportOption> *r_options) {
    String ext_filter = (get_os_name() == "Windows") ? "*.exe" : "";
    r_options->push_back(ExportOption(
            PropertyInfo(VariantType::STRING, "custom_template/release", PropertyHint::GlobalFile, ext_filter), ""));
    r_options->push_back(ExportOption(
            PropertyInfo(VariantType::STRING, "custom_template/debug", PropertyHint::GlobalFile, ext_filter), ""));

    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "binary_format/64_bits"), true));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "binary_format/embed_pck"), false));


    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "texture_format/bptc"), false));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "texture_format/s3tc"), true));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "texture_format/no_bptc_fallbacks"), true));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "binary_format/64_bits"), true));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "binary_format/embed_pck"), false));
}

const String &EditorExportPlatform::get_name() const {
    return name;
}

const String &EditorExportPlatform::get_os_name() const {
    return os_name;
}

bool EditorExportPlatform::has_valid_export_configuration(const Ref<EditorExportPreset>& p_preset, String& r_error, bool& r_missing_templates) const {
    String err;
    bool valid = false;

    // Look for export templates (first official, and if defined custom templates).

    bool use64 = p_preset->getT<bool>("binary_format/64_bits");
    bool dvalid = exists_export_template(use64 ? debug_file_64 : debug_file_32, &err);
    bool rvalid = exists_export_template(use64 ? release_file_64 : release_file_32, &err);

    if (p_preset->get("custom_template/debug") != "") {
        dvalid = FileAccess::exists(p_preset->get("custom_template/debug").as<String>());
        if (!dvalid) {
            err += TTR("Custom debug template not found.") + "\n";
        }
    }
    if (p_preset->get("custom_template/release") != "") {
        rvalid = FileAccess::exists(p_preset->get("custom_template/release").as<String>());
        if (!rvalid) {
            err += TTR("Custom release template not found.") + "\n";
        }
    }

    valid = dvalid || rvalid;
    r_missing_templates = !valid;

    if (!err.empty()) {
        r_error = err;
    }
    return valid;
}
bool EditorExportPlatform::has_valid_project_configuration(const Ref<EditorExportPreset>& p_preset, String& r_error) const {
    return true;
}

bool EditorExportPlatform::can_export(const Ref<EditorExportPreset>& p_preset, String& r_error, bool& r_missing_templates) const {
    bool valid = true;
#ifndef ANDROID_ENABLED
    String templates_error;
    valid = valid && has_valid_export_configuration(p_preset, templates_error, r_missing_templates);

    if (!templates_error.empty()) {
        r_error += templates_error;
    }
#endif

    String project_configuration_error;
    valid = valid && has_valid_project_configuration(p_preset, project_configuration_error);

    if (!project_configuration_error.empty()) {
        r_error += project_configuration_error;
    }

    return valid;
}

Vector<String> EditorExportPlatform::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
    Vector<String> list;
    for (const eastl::pair<const String, String> &E : extensions) {
        if (p_preset->getT<bool>(StringName(E.first))) {
            list.push_back(E.second);
            return list;
        }
    }
    auto iter = extensions.find_as("default");
    if (extensions.end() != iter) {
        list.push_back(iter->second);
        return list;
    }

    return list;
}

Error EditorExportPlatform::export_project(const Ref<EditorExportPreset>& p_preset, bool p_debug, StringView p_path, int p_flags) {
    ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);

    Error err = prepare_template(p_preset, p_debug, p_path, p_flags);
    if (err == OK) {
        err = modify_template(p_preset, p_debug, p_path, p_flags);
    }
    if (err == OK) {
        err = export_project_data(p_preset, p_debug, p_path, p_flags);
    }

    return err;
}

Error EditorExportPlatform::prepare_template(const Ref<EditorExportPreset>& p_preset, bool p_debug, StringView p_path, int p_flags) {
    if (!DirAccess::exists(PathUtils::get_base_dir(p_path))) {
        add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Template"), TTR("The given export path doesn't exist."));
        return ERR_FILE_BAD_PATH;
    }

    String custom_debug = p_preset->getT<String>("custom_template/debug");
    String custom_release = p_preset->getT<String>("custom_template/release");

    StringView template_path = p_debug ? custom_debug : custom_release;

    template_path = StringUtils::strip_edges(template_path);

    if (template_path.empty()) {
        if (p_preset->get("binary_format/64_bits")) {
            if (p_debug) {
                template_path = find_export_template(debug_file_64);
            }
            else {
                template_path = find_export_template(release_file_64);
            }
        }
        else {
            if (p_debug) {
                template_path = find_export_template(debug_file_32);
            }
            else {
                template_path = find_export_template(release_file_32);
            }
        }
    }

    if (!template_path.empty() && !FileAccess::exists(template_path)) {
        add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Template").asCString(), FormatVE(TTR("Template file not found: \"%.*s\".").asCString(), template_path.size(), template_path.data()));
        return ERR_FILE_NOT_FOUND;
    }

    DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    da->make_dir_recursive(PathUtils::get_base_dir(p_path));
    Error err = da->copy(template_path, p_path, get_chmod_flags());
    if (err != OK) {
        add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Template"), TTR("Failed to copy export template."));
    }

    return err;
}

Error EditorExportPlatform::export_project_data(const Ref<EditorExportPreset>& p_preset, bool p_debug, StringView p_path, int p_flags) {
    String pck_path;
    if (p_preset->get("binary_format/embed_pck")) {
        pck_path = p_path;
    }
    else {
        pck_path = String(PathUtils::get_basename(p_path)) + ".pck";
    }

    Vector<SharedObject> so_files;

    int64_t embedded_pos;
    int64_t embedded_size;
    Error err = save_pack(p_preset, pck_path, &so_files, p_preset->getT<bool>("binary_format/embed_pck"), &embedded_pos, &embedded_size);
    if (err == OK && p_preset->get("binary_format/embed_pck")) {
        if (embedded_size >= 0x100000000 && !p_preset->get("binary_format/64_bits")) {
            add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), TTR("On 32-bit exports the embedded PCK cannot be bigger than 4 GiB."));
            return ERR_INVALID_PARAMETER;
        }

        err = fixup_embedded_pck(p_path, embedded_pos, embedded_size);
    }

    if (err == OK && !so_files.empty()) {
        // If shared object files, copy them.
        DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
        for (int i = 0; i < so_files.size() && err == OK; i++) {
            String new_path = PathUtils::plus_file(PathUtils::get_base_dir(p_path), PathUtils::get_file(so_files[i].path));
            err = da->copy(so_files[i].path, new_path);
            if (err == OK) {
                err = sign_shared_object(p_preset, p_debug, new_path);
            }
        }
    }

    return err;
}
Error EditorExportPlatform::sign_shared_object(
        const Ref<EditorExportPreset> &p_preset, bool p_debug, StringView p_path) {
    return OK;
}
void EditorExportPlatform::set_extension(StringView p_extension, StringView p_feature_key) {
    extensions[String(p_feature_key)] = p_extension;
}

void EditorExportPlatform::set_name(StringView p_name) {
    name = p_name;
}

void EditorExportPlatform::set_os_name(StringView p_name) {
    os_name = p_name;
}

void EditorExportPlatform::set_logo(const Ref<Texture> &p_logo) {
    // TODO: SEGS: ImageTexture is the only supported logo type, make it explicit in function arguments.
    logo = dynamic_ref_cast<ImageTexture>(p_logo);
}

void EditorExportPlatform::set_release_64(StringView p_file) {
    release_file_64 = p_file;
}

void EditorExportPlatform::set_release_32(StringView p_file) {
    release_file_32 = p_file;
}
void EditorExportPlatform::set_debug_64(StringView p_file) {
    debug_file_64 = p_file;
}
void EditorExportPlatform::set_debug_32(StringView p_file) {
    debug_file_32 = p_file;
}

void EditorExportPlatform::add_platform_feature(StringView p_feature) {
    extra_features.insert(p_feature);
}

void EditorExportPlatform::get_platform_features(Vector<String> *r_features) {
    r_features->push_back("pc"); // all pcs support "pc"
    r_features->push_back("s3tc"); // all pcs support "s3tc" compression
    r_features->push_back(get_os_name()); // OS name is a feature
    for (const String &E : extra_features) {
        r_features->push_back(E);
    }
}

void EditorExportPlatform::resolve_platform_feature_priorities(
        const Ref<EditorExportPreset> &p_preset, Set<String> &p_features) {
    if (p_features.contains("bptc")) {
        if (p_preset->has("texture_format/no_bptc_fallbacks")) {
            p_features.erase("s3tc");
        }
    }
}

int EditorExportPlatform::get_chmod_flags() const {
    return chmod_flags;
}

void EditorExportPlatform::set_chmod_flags(int p_flags) {
    chmod_flags = p_flags;
}

///////////////////////

void EditorExportTextSceneToBinaryPlugin::_export_file(
        StringView p_path, StringView p_type, const Set<String> &p_features) {
    String extension = StringUtils::to_lower(PathUtils::get_extension(p_path));
    if (extension != "tres" && extension != "tscn") {
        return;
    }

    bool convert = GLOBAL_GET("editor/convert_text_resources_to_binary_on_export").as<bool>();
    if (!convert) {
        return;
    }
    String tmp_path(PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(), "tmpfile.res"));
    Error err = ResourceFormatLoaderText::convert_file_to_binary(p_path, tmp_path);
    if (err != OK) {
        DirAccess::remove_file_or_error(tmp_path);
        ERR_FAIL();
    }
    Vector<uint8_t> data = FileAccess::get_file_as_array(tmp_path);
    if (data.empty()) {
        DirAccess::remove_file_or_error(tmp_path);
        ERR_FAIL();
    }
    DirAccess::remove_file_or_error(tmp_path);
    add_file(String(p_path) + ".converted.res", data, true);
}

EditorExportTextSceneToBinaryPlugin::EditorExportTextSceneToBinaryPlugin() {
    GLOBAL_DEF("editor/convert_text_resources_to_binary_on_export", false);
}
