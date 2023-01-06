/*************************************************************************/
/*  project_settings.cpp                                                 */
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

#include "project_settings.h"

#include "core/bind/core_bind.h"
#include "core/core_string_names.h"
#include "core/input/input_event.h"
#include "core/io/file_access_network.h"
#include "core/io/file_access_pack.h"
#include "core/io/marshalls.h"
#include "core/method_bind.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/keyboard.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/pool_vector.h"
#include "core/string_formatter.h"
#include "core/string_utils.inl"
#include "core/variant_parser.h"

#include "EASTL/sort.h"
#include <zlib.h>

IMPL_GDCLASS(ProjectSettings)

const String ProjectSettings::PROJECT_DATA_DIR_NAME_SUFFIX = "import";
ProjectSettings *ProjectSettings::singleton = nullptr;

namespace {

static Error _save_settings_binary(StringView p_file, const HashMap<String, Vector<String>> &inp_props, const ProjectSettings::CustomMap &p_custom,
        const String &p_custom_features) {
    Error err;
    FileAccess *file = FileAccess::open(p_file, FileAccess::WRITE, &err);
    ERR_FAIL_COND_V_MSG(err != OK, err, "Couldn't save project.binary at " + String(p_file) + ".");

    uint8_t hdr[4] = { 'E', 'C', 'F', 'G' };
    file->store_buffer(hdr, 4);

    int count = 0;

    for (const eastl::pair<const String, Vector<String>> &E : inp_props) {
        count += E.second.size();
    }

    if (!p_custom_features.empty()) {
        file->store_32(count + 1);
        // store how many properties are saved, add one for custom featuers, which must always go first
        String key(CoreStringNames::get_singleton()->_custom_features);
        file->store_pascal_string(key);

        int len;
        err = encode_variant(p_custom_features, nullptr, len, false);
        if (err != OK) {
            memdelete(file);
            ERR_FAIL_V(err);
        }

        Vector<uint8_t> buff;
        buff.resize(len);

        err = encode_variant(p_custom_features, buff.data(), len, false);
        if (err != OK) {
            memdelete(file);
            ERR_FAIL_V(err);
        }
        file->store_32(len);
        file->store_buffer(buff.data(), buff.size());

    } else {
        file->store_32(count); // store how many properties are saved
    }

    for (const eastl::pair<const String, Vector<String>> &E : inp_props) {
        for (String key : E.second) {
            if (!E.first.empty())
                key = E.first + "/" + key;
            Variant value;
            StringName keyname(key);
            if (p_custom.contains(keyname))
                value = p_custom.at(keyname);
            else
                value = ProjectSettings::get_singleton()->get(keyname);

            file->store_32(key.length());
            file->store_string(key);

            int len;
            err = encode_variant(value, nullptr, len, true);
            if (err != OK)
                memdelete(file);
            ERR_FAIL_COND_V_MSG(err != OK, ERR_INVALID_DATA, "Error when trying to encode Variant.");

            Vector<uint8_t> buff;
            buff.resize(len);

            err = encode_variant(value, buff.data(), len, true);
            if (err != OK)
                memdelete(file);
            ERR_FAIL_COND_V_MSG(err != OK, ERR_INVALID_DATA, "Error when trying to encode Variant.");
            file->store_32(len);
            file->store_buffer(buff.data(), buff.size());
        }
    }

    file->close();
    memdelete(file);

    return OK;
}

static Error _save_settings_text(
        StringView p_file, const HashMap<String, Vector<String>> &props, const ProjectSettings::CustomMap &p_custom, const String &p_custom_features) {
    Error err;
    FileAccess *file = FileAccess::open(p_file, FileAccess::WRITE, &err);

    ERR_FAIL_COND_V_MSG(err != OK, err, "Couldn't save project.godot - " + String(p_file) + ".");

    file->store_line("; Engine configuration file.");
    file->store_line("; It's best edited using the editor UI and not directly,");
    file->store_line("; since the parameters that go here are not all obvious.");
    file->store_line(";");
    file->store_line("; Format:");
    file->store_line(";   [section] ; section goes between []");
    file->store_line(";   param=value ; assign values to parameters");
    file->store_line("");

    file->store_string("config_version=" + itos(ProjectSettings::CONFIG_VERSION) + "\n");
    if (!p_custom_features.empty())
        file->store_string("custom_features=\"" + p_custom_features + "\"\n");
    file->store_string("\n");

    for (auto E = props.begin(); E != props.end(); ++E) {
        if (E != props.begin())
            file->store_string("\n");

        if (!E->first.empty())
            file->store_string("[" + E->first + "]\n\n");
        for (const String &F : E->second) {
            String key = F;
            if (!E->first.empty())
                key = E->first + "/" + key;
            Variant value;
            StringName keyname(key);
            if (p_custom.contains(keyname))
                value = p_custom.at(keyname);
            else
                value = ProjectSettings::get_singleton()->get(keyname);

            String vstr;
            VariantWriter::write_to_string(value, vstr);
            file->store_string(StringUtils::property_name_encode(F) + "=" + vstr + "\n");
        }
    }

    file->close();
    memdelete(file);

    return OK;
}
static bool _convert_to_last_version(int p_from_version, HashMap<StringName, SettingsVariantContainer> &props) {
    if (p_from_version >= ProjectSettings::CONFIG_VERSION) {
        return false;
    }
    bool changed = false;
    // Converts the actions from array to dictionary (array of events to dictionary with deadzone + events)
    for (eastl::pair<const StringName, SettingsVariantContainer> &E : props) {
        Variant value = E.second.variant;
        if (StringUtils::begins_with(E.first, "input/") && value.get_type() == VariantType::ARRAY) {
            Array array = value.as<Array>();
            Dictionary action;
            action["deadzone"] = Variant(0.5f);
            action["events"] = array;
            E.second.variant = action;
            changed = true;
        }
    }
    return changed;
}
static Error _load_settings_text(StringView p_path, HashMap<StringName, SettingsVariantContainer> & props) {
    ProjectSettings *ps = ProjectSettings::get_singleton();
    Error err;
    FileAccessRef f = FileAccess::open(p_path, FileAccess::READ, &err);

    if (!f) {
        // FIXME: Above 'err' error code is ERR_FILE_CANT_OPEN if the file is missing
        // This needs to be streamlined if we want decent error reporting
        return ERR_FILE_NOT_FOUND;
    }

    VariantParserStream *stream = VariantParser::get_file_stream(f);

    Variant value;
    VariantParser::Tag next_tag;

    int lines = 0;
    String error_text;
    String section;
    int config_version = 0;

    while (true) {
        String assign = Variant().as<String>();
        next_tag.fields.clear();
        next_tag.name.clear();

        err = VariantParser::parse_tag_assign_eof(stream, lines, error_text, next_tag, assign, value, nullptr, true);
        if (err == ERR_FILE_EOF) {
            VariantParser::release_stream(stream);
            // If we're loading a project.godot from source code, we can operate some
            // ProjectSettings conversions if need be.
            _convert_to_last_version(config_version, props);
            ps->set_last_saved_time(FileAccess::get_modified_time(PathUtils::plus_file(ps->get_resource_path(), "project.godot")));
            return OK;
        }
        if (err != OK) {
            ERR_PRINTF("Error parsing %s at line %d: %s File might be corrupted.", String(p_path).c_str(), lines, error_text.c_str());
            VariantParser::release_stream(stream);
            return err;
        }

        if (!assign.empty()) {
            if (section.empty() && assign == "config_version") {
                config_version = value.as<int>();
                if (config_version > ProjectSettings::CONFIG_VERSION) {
                    VariantParser::release_stream(stream);
                    ERR_FAIL_V_MSG(
                            ERR_FILE_CANT_OPEN, FormatVE("Can't open project at '%.*s', its `config_version` (%d) is from a more recent and "
                                                         "incompatible version of the engine. Expected config version: %d.",
                                                        uint32_t(p_path.length()), p_path.data(), config_version, ProjectSettings::CONFIG_VERSION));
                }
            } else {
                if (section.empty()) {
                    ps->set(StringName(assign), value);
                } else {
                    ps->set(StringName(section + "/" + assign), value);
                }
            }
        } else if (!next_tag.name.empty()) {
            section = next_tag.name;
        }
    }
}

static Error _load_settings_binary(StringView p_path) {
    ProjectSettings *ps = ProjectSettings::get_singleton();
    Error err;
    FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);
    if (err != OK) {
        return err;
    }

    uint8_t hdr[4];
    f->get_buffer(hdr, 4);
    if (hdr[0] != 'E' || hdr[1] != 'C' || hdr[2] != 'F' || hdr[3] != 'G') {
        memdelete(f);
        ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Corrupted header in binary project.binary (not ECFG).");
    }

    uint32_t count = f->get_32();

    for (uint32_t i = 0; i < count; i++) {
        uint32_t slen = f->get_32();
        CharString cs;
        cs.resize(slen + 1);
        cs[slen] = 0;
        f->get_buffer((uint8_t *)cs.data(), slen);
        String key(cs.data());

        uint32_t vlen = f->get_32();
        Vector<uint8_t> d;
        d.resize(vlen);
        f->get_buffer(d.data(), vlen);
        Variant value;
        err = decode_variant(value, d.data(), d.size(), nullptr, true);
        ERR_CONTINUE_MSG(err != OK, "Error decoding property: " + key + ".");
        ps->set(StringName(key), value);
    }

    f->close();
    memdelete(f);
    return OK;
}
static Error _load_settings_text_or_binary(StringView p_text_path, StringView p_bin_path, HashMap<StringName, SettingsVariantContainer> &props) {
    // Attempt first to load the binary project.godot file.
    Error err = _load_settings_binary(p_bin_path);
    if (err == OK) {
        return OK;
    }
    if (err != ERR_FILE_NOT_FOUND) {
        // If the file exists but can't be loaded, we want to know it.
        ERR_PRINT("Couldn't load file '" + p_bin_path + "', error code " + itos(err) + ".");
    }

    // Fallback to text-based project.godot file if binary was not found.
    err = _load_settings_text(p_text_path, props);
    if (err == OK) {
        return OK;
    }
    if (err != ERR_FILE_NOT_FOUND) {
        ERR_PRINT("Couldn't load file '" + p_text_path + "', error code " + itos(err) + ".");
    }

    return err;
}

} // namespace

ProjectSettings *ProjectSettings::get_singleton() {
    return singleton;
}

const String &ProjectSettings::get_project_data_dir_name() const {
    return project_data_dir_name;
}

String ProjectSettings::get_project_data_path() const {
    return "res://" + get_project_data_dir_name();
}

const String &ProjectSettings::get_resource_path() const {
    return resource_path;
}

String ProjectSettings::localize_path(StringView p_path) const {
    if (resource_path.empty() || StringUtils::begins_with(p_path, "res://") || StringUtils::begins_with(p_path, "user://") ||
            (PathUtils::is_abs_path(p_path) && !StringUtils::begins_with(p_path, resource_path)))
        return PathUtils::simplify_path(p_path);

    DirAccess *dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

    String path = PathUtils::simplify_path(PathUtils::from_native_path(p_path));

    if (dir->change_dir(path) == OK) {
        String cwd = dir->get_current_dir();
        cwd = PathUtils::from_native_path(cwd);

        memdelete(dir);

        // Ensure that we end with a '/'.
        // This is important to ensure that we do not wrongly localize the resource path
        // in an absolute path that just happens to contain this string but points to a
        // different folder (e.g. "/my/project" as resource_path would be contained in
        // "/my/project_data", even though the latter is not part of res://.
        // `plus_file("")` is an easy way to ensure we have a trailing '/'.
        const String res_path = PathUtils::plus_file(resource_path, "");

        // DirAccess::get_current_dir() is not guaranteed to return a path that with a trailing '/',
        // so we must make sure we have it as well in order to compare with 'res_path'.
        cwd = PathUtils::plus_file(cwd, "");

        if (!StringUtils::begins_with(cwd, res_path)) {
            return String(p_path);
        }

        return StringUtils::replace_first(cwd, res_path, "res://");
    } else {
        memdelete(dir);

        size_t sep = StringUtils::rfind(path, "/");
        if (sep == String::npos) {
            return "res://" + path;
        }

        StringView parent = StringUtils::substr(path, 0, sep);

        String plocal = localize_path(parent);
        if (plocal.empty()) {
            return String();
        }
        // Only strip the starting '/' from 'path' if its parent ('plocal') ends with '/'
        if (plocal.back() == '/') {
            sep += 1;
        }

        return plocal + StringUtils::substr(path, int(sep), path.size() - sep);
    }
}

void ProjectSettings::set_initial_value(const StringName &p_name, const Variant &p_value) {
    ERR_FAIL_COND_MSG(!props.contains(p_name), "Request for nonexistent project setting: " + String(p_name) + ".");
    props[p_name].initial = p_value;
}
void ProjectSettings::set_restart_if_changed(const StringName &p_name, bool p_restart) {
    ERR_FAIL_COND_MSG(!props.contains(p_name), "Request for nonexistent project setting: " + String(p_name) + ".");
    props[p_name].restart_if_changed = p_restart;
}

void ProjectSettings::set_hide_from_editor(const StringName &p_name, bool p_hide_from_editor) {
    ERR_FAIL_COND_MSG(!props.contains(p_name), "Request for nonexistent project setting: " + p_name + ".");
    props[p_name].hide_from_editor = p_hide_from_editor;
}

void ProjectSettings::set_ignore_value_in_docs(const StringName &p_name, bool p_ignore) {
    ERR_FAIL_COND_MSG(!props.contains(p_name), "Request for nonexistent project setting: " + p_name + ".");
#ifdef DEBUG_METHODS_ENABLED
    props[p_name].ignore_value_in_docs = p_ignore;
#endif
}

bool ProjectSettings::get_ignore_value_in_docs(const StringName &p_name) const {
    ERR_FAIL_COND_V_MSG(!props.contains(p_name), false, "Request for nonexistent project setting: " + p_name + ".");
#ifdef DEBUG_METHODS_ENABLED
    return props.at(p_name).ignore_value_in_docs;
#else
    return false;
#endif
}

String ProjectSettings::globalize_path(StringView p_path) const {
    if (StringUtils::begins_with(p_path, "res://")) {
        if (!resource_path.empty()) {
            return StringUtils::replace(p_path, "res:/", resource_path);
        }
        return StringUtils::replace(p_path, "res://", "");
    } else if (StringUtils::begins_with(p_path, "user://")) {
        String data_dir = OS::get_singleton()->get_user_data_dir();
        if (!data_dir.empty()) {
            return StringUtils::replace(p_path, "user:/", data_dir);
        }
        return StringUtils::replace(p_path, "user://", "");
    }

    return String(p_path);
}

void ProjectSettings::update() {
    if (_dirty_this_frame) {
        // A signal is sent a single time at the end of the frame when project settings
        // are changed. This allows objects to respond.
        // Alternatively objects outside the signal system can query ProjectSettings::has_changes()
        if (_dirty_this_frame == 2) {
            emit_signal("project_settings_changed");
        }

        _dirty_this_frame--;
    }
}
bool ProjectSettings::_set(const StringName &p_name, const Variant &p_value) {
    _THREAD_SAFE_METHOD_;

    // marking the project settings as dirty allows them only to be
    // checked when dirty.
    _dirty_this_frame = 2;

    if (p_value.get_type() == VariantType::NIL) {
        props.erase(p_name);
        return true;
    }

    if (p_name == CoreStringNames::get_singleton()->_custom_features) {
        String val_str(p_value.as<String>());
        Vector<StringView> custom_feature_array = StringUtils::split(val_str, ',');
        for (StringView part : custom_feature_array) {
            custom_features.insert(part);
        }
        return true;
    }

    if (!disable_feature_overrides) {
        auto dot = StringUtils::find(p_name, ".");
        if (dot != String::npos) {
            Vector<StringView> s = StringUtils::split(p_name, '.');

            bool override_valid = false;
            for (size_t i = 1; i < s.size(); i++) {
                StringView feature = StringUtils::strip_edges(s[i]);
                if (OS::get_singleton()->has_feature(feature) || custom_features.contains_as(feature)) {
                    override_valid = true;
                    break;
                }
            }

            if (override_valid) {
                feature_overrides[StringName(s[0])] = p_name;
            }
        }
    }

    if (props.contains(p_name)) {
        if (!props[p_name].overridden) {
            props[p_name].variant = p_value;
        }
    } else {
        props[p_name] = SettingsVariantContainer(p_value, last_order++);
    }

    return true;
}
bool ProjectSettings::_get(const StringName &p_name, Variant &r_ret) const {
    _THREAD_SAFE_METHOD_;

    StringName name = p_name;
    if (!disable_feature_overrides && feature_overrides.contains(name)) {
        name = feature_overrides.at(name);
    }
    if (!props.contains(name)) {
        WARN_PRINT("Property not found: " + String(name));
        return false;
    }
    r_ret = props.at(name).variant;
    return true;
}

struct _VCSort {
    StringName name;
    VariantType type;
    int order;
    int flags;

    bool operator<(const _VCSort &p_vcs) const {
        return order == p_vcs.order ? name < p_vcs.name : order < p_vcs.order;
    }
};

void ProjectSettings::_get_property_list(Vector<PropertyInfo> *p_list) const {
    _THREAD_SAFE_METHOD_;
    using namespace StringUtils;
    Set<_VCSort> vclist;

    for (const eastl::pair<const StringName, SettingsVariantContainer> &E : props) {
        const SettingsVariantContainer *v = &E.second;

        if (v->hide_from_editor) {
            continue;
        }

        _VCSort vc;
        vc.name = E.first;
        vc.order = v->order;
        vc.type = v->variant.get_type();
        if (begins_with(vc.name, "input/") || begins_with(vc.name, "import/") || begins_with(vc.name, "export/") ||
                begins_with(vc.name, "/remap") || begins_with(vc.name, "/locale") ||
                begins_with(vc.name, "/autoload")) {
            vc.flags = PROPERTY_USAGE_STORAGE;
        }
        else {
            vc.flags = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
        }

        if (v->restart_if_changed) {
            vc.flags |= PROPERTY_USAGE_RESTART_IF_CHANGED;
        }
        vclist.insert(vc);
    }

    for (auto & iter : vclist) {
        _VCSort E(eastl::move(iter));
        StringName prop_info_name = E.name;
        auto dot = find(prop_info_name, ".");
        if (dot != String::npos) {
            prop_info_name = StringName(StringView(prop_info_name).substr(0, dot));
        }

        if (custom_prop_info.contains(prop_info_name)) {
            PropertyInfo pi = custom_prop_info.at(prop_info_name);
            pi.name = E.name;
            pi.usage = E.flags;
            p_list->emplace_back(eastl::move(pi));
        } else {
            p_list->emplace_back(E.type, eastl::move(E.name), PropertyHint::None, nullptr, E.flags);
        }
    }
}

bool ProjectSettings::_load_resource_pack(StringView p_pack, bool p_replace_files) {
    if (PackedData::get_singleton()->is_disabled()) {
        return false;
    }

    bool ok = PackedData::get_singleton()->add_pack(p_pack, p_replace_files) == OK;

    if (!ok) {
        return false;
    }

    // if data.pck is found, all directory access will be from here
    DirAccess::make_default<DirAccessPack>(DirAccess::ACCESS_RESOURCES);
    using_datapack = true;

    return true;
}

/*
 * This method is responsible for loading a project.godot file and/or data file
 * using the following merit order:
 *  - If using NetworkClient, try to lookup project file or fail.
 *  - If --main_pack was passed by the user (`p_main_pack`), load it or fail.
 *  - Search for .pck file matching binary name. There are two possibilities:
 *    o PathUtils::get_basename(exec_path) + '.pck' (e.g. 'win_game.exe' -> 'win_game.pck')
 *    o exec_path + '.pck' (e.g. 'linux_game' -> 'linux_game.pck')
 *    For each tentative, if the file exists, load it or fail.
 *  - On relevant platforms (Android/iOS), lookup project file in OS resource path.
 *    If found, load it or fail.
 *  - Lookup project file in passed `p_path` (--path passed by the user), i.e. we
 *    are running from source code.
 *    If not found and `p_upwards` is true (--upwards passed by the user), look for
 *    project files in parent folders up to the system root (used to run a game
 *    from command line while in a subfolder).
 *    If a project file is found, load it or fail.
 *    If nothing was found, error out.
 */
Error ProjectSettings::_setup(StringView p_path, StringView p_main_pack, bool p_upwards, bool p_ignore_override) {
    using namespace PathUtils;
    if (!OS::get_singleton()->get_resource_dir().empty()) {
        // OS will call ProjectSettings->get_resource_path which will be empty if not overridden!
        // If the OS would rather use a specific location, then it will not be empty.
        resource_path = OS::get_singleton()->get_resource_dir().replaced("\\", "/");
        if (!resource_path.empty() && resource_path.ends_with('/')) {
            resource_path = resource_path.substr(0, resource_path.length() - 1); // Chop end.
        }
    }
    // If looking for files in a network client, use it directly

    if (FileAccessNetworkClient::get_singleton()) {
        Error err = _load_settings_text_or_binary("res://project.godot", "res://project.binary", props);
        if (err == OK && !p_ignore_override) {
            // Optional, we don't mind if it fails
            _load_settings_text("res://override.cfg", props);
        }
        return err;
    }

    // Attempt with a user-defined main pack first

    if (!p_main_pack.empty()) {
        bool ok = _load_resource_pack(p_main_pack);
        ERR_FAIL_COND_V_MSG(!ok, ERR_CANT_OPEN, "Cannot open resource pack '" + String(p_main_pack) + "'.");

        Error err = _load_settings_text_or_binary("res://project.godot", "res://project.binary", props);
        if (err == OK && !p_ignore_override) {
            // Load override from location of the main pack
            // Optional, we don't mind if it fails
            _load_settings_text(plus_file(get_base_dir(p_main_pack), "override.cfg"), props);
        }
        return err;
    }

    String exec_path = OS::get_singleton()->get_executable_path();

    if (!exec_path.empty()) {
        // Attempt with exec_name.pck
        // (This is the usual case when distributing a Godot game.)

        // Based on the OS, it can be the exec path + '.pck' (Linux w/o extension, macOS in .app bundle)
        // or the exec path's basename + '.pck' (Windows).
        // We need to test both possibilities as extensions for Linux binaries are optional
        // (so both 'mygame.bin' and 'mygame' should be able to find 'mygame.pck').

        String exec_dir = get_base_dir(exec_path);
        String exec_filename(get_file(exec_path));
        String exec_basename(get_basename(exec_filename));

        // Attempt with PCK bundled into executable
        bool found = _load_resource_pack(exec_path);

#ifdef OSX_ENABLED
        if (!found) {
            // Attempt to load PCK from macOS .app bundle resources
            found = _load_resource_pack(
                    plus_file(OS::get_singleton()->get_bundle_resource_dir(), exec_basename + ".pck"));
        }
#endif

        if (!found) {
            // Try to load data pack at the location of the executable
            // As mentioned above, we have two potential names to attempt
            found = _load_resource_pack(plus_file(exec_dir, exec_basename + ".pck")) ||
                    _load_resource_pack(plus_file(exec_dir, exec_filename + ".pck"));

            if (!found) {
                // If we couldn't find them next to the executable, we attempt
                // the current working directory. Same story, two tests.
                found = _load_resource_pack(exec_basename + ".pck") || _load_resource_pack(exec_filename + ".pck");
            }
        }

        // If we opened our package, try and load our project
        if (found) {
            Error err = _load_settings_text_or_binary("res://project.godot", "res://project.binary", props);
            if (err == OK && !p_ignore_override) {
                // Load override from location of executable
                // Optional, we don't mind if it fails
                _load_settings_text(plus_file(get_base_dir(exec_path), "override.cfg"), props);
            }
            return err;
        }
    }

    // Try to use the filesystem for files, according to OS. (only Android -when reading from pck- and iOS use this)

    if (!OS::get_singleton()->get_resource_dir().empty()) {

        Error err = _load_settings_text_or_binary("res://project.godot", "res://project.binary",props);
        if (err == OK && !p_ignore_override) {
            // Optional, we don't mind if it fails
            _load_settings_text("res://override.cfg", props);
        }
        return err;
    }

    // Nothing was found, try to find a project file in provided path (`p_path`)
    // or, if requested (`p_upwards`) in parent directories.

    DirAccess *d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    ERR_FAIL_COND_V_MSG(!d, ERR_CANT_CREATE, "Cannot create DirAccess for path '" + String(p_path) + "'.");
    d->change_dir(p_path);

    String current_dir = d->get_current_dir();
    String candidate = current_dir;
    bool found = false;
    Error err;

    while (true) {
        err = _load_settings_text_or_binary(plus_file(current_dir, "project.godot"), plus_file(current_dir, "project.binary"), props);
        if (err == OK && !p_ignore_override) {
            // Optional, we don't mind if it fails
            _load_settings_text(plus_file(current_dir, "override.cfg"), props);
            candidate = current_dir;
            found = true;
            break;
        }

        if (p_upwards) {
            // Try to load settings ascending through parent directories
            d->change_dir("..");
            if (d->get_current_dir() == current_dir)
                break; // not doing anything useful
            current_dir = d->get_current_dir();
        } else {
            break;
        }
    }

    resource_path = candidate;
    resource_path = from_native_path(resource_path); // windows path to unix path just in case
    memdelete(d);

    if (!found)
        return err;

    if (resource_path.length() && resource_path.back() == '/')
        resource_path = StringUtils::substr(resource_path, 0, resource_path.length() - 1); // chop end

    return OK;
}

Error ProjectSettings::setup(StringView p_path, StringView p_main_pack, bool p_upwards, bool p_ignore_override) {
    Error err = _setup(p_path, p_main_pack, p_upwards, p_ignore_override);
    if (err == OK) {
        String custom_settings = T_GLOBAL_DEF<String>("application/config/project_settings_override", "");
        if (!custom_settings.empty()) {
            _load_settings_text(custom_settings, props);
        }
    }
    // Updating the default value after the project settings have loaded.
    bool use_hidden_directory = T_GLOBAL_GET<bool>("application/config/use_hidden_project_data_directory");
    project_data_dir_name = (use_hidden_directory ? "." : "") + PROJECT_DATA_DIR_NAME_SUFFIX;

    return err;
}

bool ProjectSettings::has_setting(const StringName &p_var) const {
    _THREAD_SAFE_METHOD_;

    return props.contains(p_var);
}

void ProjectSettings::set_registering_order(bool p_enable) {
    registering_order = p_enable;
}

int ProjectSettings::get_order(const StringName &p_name) const {
    ERR_FAIL_COND_V_MSG(
            !props.contains(p_name), -1, "Request for nonexistent project setting: " + String(p_name) + ".");
    return props.at(p_name).order;
}

void ProjectSettings::set_order(const StringName &p_name, int p_order) {
    ERR_FAIL_COND_MSG(!props.contains(p_name), "Request for nonexistent project setting: " + String(p_name) + ".");
    props[p_name].order = p_order;
}

void ProjectSettings::set_builtin_order(const StringName &p_name) {
    ERR_FAIL_COND_MSG(!props.contains(p_name), "Request for nonexistent project setting: " + String(p_name) + ".");
    if (props[p_name].order >= NO_BUILTIN_ORDER_BASE) {
        props[p_name].order = last_builtin_order++;
    }
}

void ProjectSettings::clear(const StringName &p_name) {
    ERR_FAIL_COND_MSG(!props.contains(p_name), "Request for nonexistent project setting: " + String(p_name) + ".");
    props.erase(p_name);
}

Error ProjectSettings::save() {
    String project_path(PathUtils::plus_file(get_resource_path(), "project.godot"));
    Error error = save_custom(project_path);
    if (error == OK) {
        last_save_time = FileAccess::get_modified_time(project_path);
    }
    return error;
}

Error ProjectSettings::_save_custom_bnd(StringView p_file) { // add other params as dictionary and array?

    return save_custom(p_file);
}

Error ProjectSettings::save_custom(StringView p_path, const CustomMap &p_custom,
        const Vector<String> &p_custom_features, bool p_merge_with_current) {
    ERR_FAIL_COND_V_MSG(p_path.empty(), ERR_INVALID_PARAMETER, "Project settings save path cannot be empty.");

    Set<_VCSort> vclist;

    if (p_merge_with_current) {
        for (eastl::pair<const StringName, SettingsVariantContainer> &G : props) {
            const SettingsVariantContainer *v = &G.second;

            if (v->hide_from_editor)
                continue;

            if (p_custom.contains(G.first))
                continue;

            _VCSort vc;
            vc.name = G.first; //*k;
            vc.order = v->order;
            vc.type = v->variant.get_type();
            vc.flags = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
            if (v->variant == v->initial)
                continue;

            vclist.insert(vc);
        }
    }

    for (const eastl::pair<const StringName, Variant> &E : p_custom) {
        // Lookup global prop to store in the same order
        HashMap<StringName, SettingsVariantContainer>::iterator global_prop = props.find(E.first);

        _VCSort vc;
        vc.name = E.first;
        vc.order = global_prop != props.end() ? global_prop->second.order : 0xFFFFFFF;
        vc.type = E.second.get_type();
        vc.flags = PROPERTY_USAGE_STORAGE;
        vclist.insert(vc);
    }

    HashMap<String, Vector<String>> props;

    for (const _VCSort &E : vclist) {
        String category = E.name.asCString();
        String name = E.name.asCString();

        auto div = StringUtils::find(category, "/");

        if (div == String::npos)
            category = "";
        else {
            category = StringUtils::substr(category, 0, div);
            name = StringUtils::substr(name, div + 1);
        }
        props[category].push_back(name);
    }

    String custom_features;

    for (size_t i = 0; i < p_custom_features.size(); i++) {
        if (i > 0)
            custom_features += ',';

        String f = StringUtils::replace(StringUtils::strip_edges(p_custom_features[i]), "\"", "");
        custom_features += f;
    }

    if (p_path.ends_with(".godot") || p_path.ends_with("override.cfg")) {
        return _save_settings_text(p_path, props, p_custom, custom_features);
    }
    if (p_path.ends_with(".binary")) {
        return _save_settings_binary(p_path, props, p_custom, custom_features);
    }

    ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Unknown config file format: " + p_path + ".");
}

Variant _GLOBAL_DEF(const StringName &p_var, const Variant &p_default, bool p_restart_if_changed, bool p_ignore_value_in_docs) {
    auto ps = ProjectSettings::get_singleton();
    if (!ps->has_setting(p_var)) {
        ps->set(p_var, p_default);
    }
    Variant ret = ps->get(p_var);

    ps->set_initial_value(p_var, p_default);
    ps->set_builtin_order(p_var);
    ps->set_restart_if_changed(p_var, p_restart_if_changed);
    ps->set_ignore_value_in_docs(p_var, p_ignore_value_in_docs);
    return ret;
}
Vector<String> ProjectSettings::get_optimizer_presets() const {
    Vector<PropertyInfo> pi;
    get_singleton()->get_property_list(&pi);
    Vector<String> names;

    for (const PropertyInfo &E : pi) {
        if (!StringUtils::begins_with(E.name, "optimizer_presets/"))
            continue;
        names.emplace_back(StringUtils::get_slice(E.name, '/', 1));
    }
    sort(names.begin(), names.end());

    return names;
}

void ProjectSettings::_add_property_info_bind(const Dictionary &p_info) {
    ERR_FAIL_COND(!p_info.has("name"));
    ERR_FAIL_COND(!p_info.has("type"));

    PropertyInfo pinfo;
    pinfo.name = p_info["name"].as<StringName>();
    ERR_FAIL_COND(!props.contains(pinfo.name));
    pinfo.type = p_info["type"].as<VariantType>();
    ERR_FAIL_INDEX(int(pinfo.type), int(VariantType::VARIANT_MAX));

    if (p_info.has("hint"))
        pinfo.hint = p_info["hint"].as<PropertyHint>();
    if (p_info.has("hint_string"))
        pinfo.hint_string = p_info["hint_string"].as<String>();

    set_custom_property_info(StringName(pinfo.name), pinfo);
}

void ProjectSettings::set_custom_property_info(const StringName &p_prop, const PropertyInfo &p_info) {
    ERR_FAIL_COND(!props.contains(p_prop));
    custom_prop_info[p_prop] = p_info;
    custom_prop_info[p_prop].name = p_prop;
}

const HashMap<StringName, PropertyInfo> &ProjectSettings::get_custom_property_info() const {
    return custom_prop_info;
}

void ProjectSettings::set_disable_feature_overrides(bool p_disable) {
    disable_feature_overrides = p_disable;
}

bool ProjectSettings::is_using_datapack() const {
    return using_datapack;
}
struct CompareStringAndStringName {
    bool operator()(const StringName &a, StringView b) const { return StringView(a) < b; }
    bool operator()(StringView a, const StringName &b) const { return a < StringView(b); }
};

bool ProjectSettings::property_can_revert(StringView p_name) {
    auto iter = props.find_as(p_name, eastl::hash<StringView>(), CompareStringAndStringName());
    if (iter == props.end())
        return false;

    return iter->second.initial != iter->second.variant;
}

Variant ProjectSettings::property_get_revert(StringView p_name) {
    auto iter = props.find_as(p_name, eastl::hash<StringView>(), CompareStringAndStringName());
    if (iter == props.end())
        return Variant();

    return iter->second.initial;
}

void ProjectSettings::set_setting(const StringName &p_setting, const Variant &p_value) {
    set(p_setting, p_value);
}

Variant ProjectSettings::get_setting(const StringName &p_setting) const {
    return get(StringName(p_setting));
}
bool ProjectSettings::has_custom_feature(StringView p_feature) const {
    return custom_features.find_as(p_feature) != custom_features.end();
}

void ProjectSettings::_bind_methods() {
    SE_BIND_METHOD(ProjectSettings,has_setting);
    SE_BIND_METHOD(ProjectSettings,set_setting);
    SE_BIND_METHOD(ProjectSettings,get_setting);
    SE_BIND_METHOD(ProjectSettings,set_order);
    SE_BIND_METHOD(ProjectSettings,get_order);
    SE_BIND_METHOD(ProjectSettings,set_initial_value);
    SE_BIND_METHOD_WRAPPER(ProjectSettings, add_property_info, _add_property_info_bind);
    SE_BIND_METHOD(ProjectSettings,clear);
    SE_BIND_METHOD(ProjectSettings,localize_path);
    SE_BIND_METHOD(ProjectSettings,globalize_path);
    SE_BIND_METHOD(ProjectSettings,save);
    MethodBinder::bind_method(D_METHOD("load_resource_pack", { "pack", "replace_files" }), &ProjectSettings::_load_resource_pack, { Variant(true) });
    SE_BIND_METHOD(ProjectSettings,property_can_revert);
    SE_BIND_METHOD(ProjectSettings,property_get_revert);

    SE_BIND_METHOD_WRAPPER(ProjectSettings, save_custom,_save_custom_bnd);

    ADD_SIGNAL(MethodInfo("project_settings_changed"));
}
static void add_key_event(Array &tgt, KeyList entry) {
    auto key = make_ref_counted<InputEventKey>();
    key->set_keycode(entry);
    tgt.emplace_back(key);
}
ProjectSettings::ProjectSettings() {
    // Initialization of engine variables should be done in the setup() method,
    // so that the values can be overridden from project.godot or project.binary.


    singleton = this;
    last_order = NO_BUILTIN_ORDER_BASE;
    last_builtin_order = 0;
    disable_feature_overrides = false;
    registering_order = true;

    GLOBAL_DEF("application/config/name", "");
    GLOBAL_DEF("application/config/description", "");
    custom_prop_info[StaticCString("application/config/description")] =
            PropertyInfo(VariantType::STRING, "application/config/description", PropertyHint::MultilineText);
    GLOBAL_DEF("application/run/main_scene", "");
    custom_prop_info[StaticCString("application/run/main_scene")] =
            PropertyInfo(VariantType::STRING, "application/run/main_scene", PropertyHint::File, "*.tscn,*.scn,*.res");
    GLOBAL_DEF("application/run/disable_stdout", false);
    GLOBAL_DEF("application/run/disable_stderr", false);
    GLOBAL_DEF_RST("application/config/use_hidden_project_data_directory", true);
    GLOBAL_DEF("application/config/use_custom_user_dir", false);
    GLOBAL_DEF("application/config/custom_user_dir_name", "");
    GLOBAL_DEF("application/config/project_settings_override", "");
    GLOBAL_DEF("display/window/size/width", 1024);
    set_custom_property_info("display/window/size/width",
    PropertyInfo(VariantType::INT, "display/window/size/width", PropertyHint::Range, "0,7680,1,or_greater")); // 8K resolution
    GLOBAL_DEF("display/window/size/height", 600);
    set_custom_property_info("display/window/size/height",
        PropertyInfo(VariantType::INT, "display/window/size/height", PropertyHint::Range, "0,4320,1,or_greater")); // 8K resolution
    GLOBAL_DEF("display/window/size/resizable", true);
    GLOBAL_DEF("display/window/size/borderless", false);
    GLOBAL_DEF("display/window/size/fullscreen", false);
    GLOBAL_DEF("display/window/size/always_on_top", false);
    GLOBAL_DEF("display/window/size/test_width", 0);
    set_custom_property_info("display/window/size/test_width",
            PropertyInfo(VariantType::INT, "display/window/size/test_width", PropertyHint::Range, "0,7680,1,or_greater")); // 8K resolution
    GLOBAL_DEF("display/window/size/test_height", 0);
    set_custom_property_info("display/window/size/test_height",
            PropertyInfo(VariantType::INT, "display/window/size/test_height", PropertyHint::Range, "0,4320,1,or_greater")); // 8K resolution
    GLOBAL_DEF("audio/default_bus_layout", "res://default_bus_layout.tres");
    custom_prop_info[StaticCString("audio/default_bus_layout")] = PropertyInfo(VariantType::STRING, "audio/default_bus_layout", PropertyHint::File, "*.tres");
    GLOBAL_DEF_RST("audio/2d_panning_strength", 1.0f);
    custom_prop_info["audio/2d_panning_strength"] = PropertyInfo(VariantType::FLOAT, "audio/2d_panning_strength", PropertyHint::Range, "0,4,0.01");
    GLOBAL_DEF_RST("audio/3d_panning_strength", 1.0f);
    custom_prop_info["audio/3d_panning_strength"] = PropertyInfo(VariantType::FLOAT, "audio/3d_panning_strength", PropertyHint::Range, "0,4,0.01");

    PoolStringArray extensions;
    if (Engine::get_singleton()->has_singleton("GodotSharp"))
        extensions.push_back("cs");
    extensions.push_back("gdshader");
    extensions.push_back("shader");
    GLOBAL_DEF("editor/main_run_args", "");
    GLOBAL_DEF("editor/scene_naming", 0); // Sync enum values with EditorNode.
    set_custom_property_info(
            "editor/scene_naming", PropertyInfo(VariantType::INT, "editor/scene_naming", PropertyHint::Enum, "Auto,PascalCase,snake_case"));

    GLOBAL_DEF("editor/search_in_file_extensions", extensions);
    custom_prop_info[StaticCString("editor/search_in_file_extensions")] =
            PropertyInfo(VariantType::POOL_STRING_ARRAY, "editor/search_in_file_extensions");

    GLOBAL_DEF("editor/script_templates_search_path", "res://script_templates");
    custom_prop_info[StaticCString("editor/script_templates_search_path")] =
            PropertyInfo(VariantType::STRING, "editor/script_templates_search_path", PropertyHint::Dir);
    GLOBAL_DEF("editor/version_control_autoload_on_startup", false);
    GLOBAL_DEF("editor/version_control_plugin_name", "");

    Dictionary action;
    action["deadzone"] = Variant(0.5f);
    Array events;
    add_key_event(events, KEY_ENTER);
    add_key_event(events, KEY_KP_ENTER);
    add_key_event(events, KEY_SPACE);

    Ref joyb(make_ref_counted<InputEventJoypadButton>());
    joyb->set_button_index(JOY_BUTTON_0);
    events.emplace_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_accept", action);
    input_presets.emplace_back("input/ui_accept");

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    add_key_event(events, KEY_SPACE);
    joyb = make_ref_counted<InputEventJoypadButton>();
    joyb->set_button_index(JOY_BUTTON_3);
    events.push_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_select", action);
    input_presets.emplace_back("input/ui_select");

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    add_key_event(events, KEY_ESCAPE);
    joyb = make_ref_counted<InputEventJoypadButton>();
    joyb->set_button_index(JOY_BUTTON_1);
    events.push_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_cancel", action);
    input_presets.emplace_back("input/ui_cancel");

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    add_key_event(events, KEY_TAB);
    action["events"] = events;
    GLOBAL_DEF("input/ui_focus_next", action);
    input_presets.emplace_back("input/ui_focus_next");

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    Ref<InputEventKey> key = make_ref_counted<InputEventKey>();
    key->set_keycode(KEY_TAB);
    key->set_shift(true);
    events.push_back(key);
    action["events"] = events;
    GLOBAL_DEF("input/ui_focus_prev", action);
    input_presets.emplace_back("input/ui_focus_prev");

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    add_key_event(events, KEY_LEFT);
    joyb = make_ref_counted<InputEventJoypadButton>();
    joyb->set_button_index(JOY_DPAD_LEFT);
    events.push_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_left", action);
    input_presets.emplace_back("input/ui_left");

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    add_key_event(events, KEY_RIGHT);
    joyb = make_ref_counted<InputEventJoypadButton>();
    joyb->set_button_index(JOY_DPAD_RIGHT);
    events.push_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_right", action);
    input_presets.emplace_back("input/ui_right");

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    add_key_event(events, KEY_UP);
    joyb = make_ref_counted<InputEventJoypadButton>();
    joyb->set_button_index(JOY_DPAD_UP);
    events.push_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_up", action);
    input_presets.emplace_back("input/ui_up");

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    add_key_event(events, KEY_DOWN);
    joyb = make_ref_counted<InputEventJoypadButton>();
    joyb->set_button_index(JOY_DPAD_DOWN);
    events.push_back(joyb);
    action["events"] = events;
    GLOBAL_DEF("input/ui_down", action);
    input_presets.emplace_back("input/ui_down");

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    add_key_event(events, KEY_PAGEUP);
    action["events"] = events;
    GLOBAL_DEF("input/ui_page_up", action);
    input_presets.emplace_back("input/ui_page_up");

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    add_key_event(events, KEY_PAGEDOWN);
    action["events"] = events;
    GLOBAL_DEF("input/ui_page_down", action);
    input_presets.emplace_back("input/ui_page_down");

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    add_key_event(events, KEY_HOME);
    action["events"] = events;
    GLOBAL_DEF("input/ui_home", action);
    input_presets.emplace_back("input/ui_home");

    action = Dictionary();
    action["deadzone"] = Variant(0.5f);
    events = Array();
    add_key_event(events, KEY_END);
    action["events"] = events;
    GLOBAL_DEF("input/ui_end", action);
    input_presets.emplace_back("input/ui_end");

    custom_prop_info[StaticCString("rendering/threads/thread_model")] = PropertyInfo(VariantType::INT,
            "rendering/threads/thread_model", PropertyHint::Enum, "Single-Unsafe,Single-Safe,Multi-Threaded");
    custom_prop_info[StaticCString("physics/2d/thread_model")] = PropertyInfo(VariantType::INT,
            "physics/2d/thread_model", PropertyHint::Enum, "Single-Unsafe,Single-Safe,Multi-Threaded");
    custom_prop_info[StaticCString("rendering/quality/intended_usage/framebuffer_allocation")] =
            PropertyInfo(VariantType::INT, "rendering/quality/intended_usage/framebuffer_allocation",
                    PropertyHint::Enum, "2D,2D Without Sampling,3D,3D Without Effects");
    // Required to make the project setting appear even if the physics engine is GodotPhysics,
    // while also making it appear in the ProjectSettings class documentation.
    GLOBAL_DEF("physics/3d/smooth_trimesh_collision", false);

    GLOBAL_DEF("rendering/quality/filters/sharpen_intensity", 0.0);
    custom_prop_info["rendering/quality/filters/sharpen_intensity"] = PropertyInfo(VariantType::FLOAT, "rendering/quality/filters/sharpen_intensity", PropertyHint::Range, "0,1");

    GLOBAL_DEF("debug/settings/profiler/max_functions", 16384);
    custom_prop_info[StaticCString("debug/settings/profiler/max_functions")] =
            PropertyInfo(VariantType::INT, "debug/settings/profiler/max_functions", PropertyHint::Range, "128,65535,1");

    // assigning here, because using GLOBAL_GET on every block for compressing can be slow
    Compression::zstd_long_distance_matching =
            GLOBAL_DEF("compression/formats/zstd/long_distance_matching", false).as<bool>();
    custom_prop_info[StaticCString("compression/formats/zstd/long_distance_matching")] =
            PropertyInfo(VariantType::BOOL, "compression/formats/zstd/long_distance_matching");
    Compression::zstd_level = T_GLOBAL_DEF<int>("compression/formats/zstd/compression_level", 3);
    custom_prop_info[StaticCString("compression/formats/zstd/compression_level")] =
            PropertyInfo(VariantType::INT, "compression/formats/zstd/compression_level", PropertyHint::Range, "1,22,1");
    Compression::zstd_window_log_size = T_GLOBAL_DEF<int>("compression/formats/zstd/window_log_size", 27);
    custom_prop_info[StaticCString("compression/formats/zstd/window_log_size")] =
            PropertyInfo(VariantType::INT, "compression/formats/zstd/window_log_size", PropertyHint::Range, "10,30,1");

    Compression::zlib_level = T_GLOBAL_DEF<int>("compression/formats/zlib/compression_level", Z_DEFAULT_COMPRESSION);
    custom_prop_info[StaticCString("compression/formats/zlib/compression_level")] =
            PropertyInfo(VariantType::INT, "compression/formats/zlib/compression_level", PropertyHint::Range, "-1,9,1");

    Compression::gzip_level = T_GLOBAL_DEF<int>("compression/formats/gzip/compression_level", Z_DEFAULT_COMPRESSION);
    custom_prop_info[StaticCString("compression/formats/gzip/compression_level")] =
            PropertyInfo(VariantType::INT, "compression/formats/gzip/compression_level", PropertyHint::Range, "-1,9,1");

    using_datapack = false;
}

ProjectSettings::~ProjectSettings() {
    singleton = nullptr;
}
