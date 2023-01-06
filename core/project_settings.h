/*************************************************************************/
/*  project_settings.h                                                   */
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

#include "core/object.h"
#include "core/os/thread_safe.h"
#include "core/property_info.h"
#include "core/set.h"
#include "core/hash_map.h"
#include "core/string.h"
#include "core/string_name.h"
#include "core/list.h"

struct PropertyInfo;

struct SettingsVariantContainer {
    Variant variant;
    Variant initial;
    int order = 0;
    bool persist = false;
    bool hide_from_editor = false;
    bool overridden = false;
    bool restart_if_changed = false;
    bool ignore_value_in_docs = false;
    SettingsVariantContainer() = default;

    SettingsVariantContainer(Variant p_variant, int p_order, bool p_persist = false) :
            variant(eastl::move(p_variant)), order(p_order), persist(p_persist) {}
};


// Querying ProjectSettings is usually done at startup.
// Additionally, in order to keep track of changes to ProjectSettings,
// instead of Querying all the strings every frame just in case of changes,
// there is a signal "project_settings_changed" which objects can subscribe to.

// E.g. (from another Godot object)
// // Call your user written object function to Query the project settings once at creation,
// perhaps in an ENTER_TREE notification:
// _project_settings_changed()
// // Then connect your function to the signal so it is called every time something changes in future:
// ProjectSettings::get_singleton()->connect("project_settings_changed", this, "_project_settings_changed");

// Where for example your function may take the form:
// void _project_settings_changed() {
// _shadowmap_size = GLOBAL_GET("rendering/quality/shadow_atlas/size");
// }

// You may want to also disconnect from the signal in EXIT_TREE notification, if your object may be deleted
// before ProjectSettings:
// ProjectSettings::get_singleton()->disconnect("project_settings_changed", this, "_project_settings_changed");

// Additionally, for objects that are not regular Godot objects capable of subscribing to signals (e.g. Rasterizers),
// you can also query the function "has_changes()" each frame,
// and update your local settings whenever this is set.

class GODOT_EXPORT ProjectSettings : public Object {

    GDCLASS(ProjectSettings,Object)

    _THREAD_SAFE_CLASS_

    int _dirty_this_frame = 2;
public:
    using CustomMap = HashMap<StringName, Variant>;
    static const String PROJECT_DATA_DIR_NAME_SUFFIX;

    enum {
        //properties that are not for built in values begin from this value, so builtin ones are displayed first
        NO_BUILTIN_ORDER_BASE = 1 << 16
    };

protected:
    friend class ProjectSettingsPrivate;
    HashMap<StringName, SettingsVariantContainer> props;
    String resource_path;
    HashMap<StringName, PropertyInfo> custom_prop_info;
    Vector<String> input_presets;

    Set<String> custom_features;
    HashMap<StringName, StringName> feature_overrides;
    String project_data_dir_name;
    uint64_t last_save_time = 0;
    int last_order;
    int last_builtin_order;
    bool registering_order;
    bool disable_feature_overrides;
    bool using_datapack;

    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(Vector<PropertyInfo> *p_list) const;

    static ProjectSettings *singleton;
protected:
    static void _bind_methods();
public:

    Error _save_custom_bnd(StringView p_file);

    bool _load_resource_pack(StringView p_pack, bool p_replace_files = true);

    void _add_property_info_bind(const Dictionary &p_info);

    Error _setup(StringView p_path, StringView p_main_pack, bool p_upwards = false, bool p_ignore_override=false);

public:
    static const int CONFIG_VERSION = 4;

    void set_setting(const StringName &p_setting, const Variant &p_value);
    Variant get_setting(const StringName &p_setting) const;

    bool has_setting(const StringName &p_var) const;
    String localize_path(StringView p_path) const;
    String globalize_path(StringView p_path) const;

    void set_initial_value(const StringName &p_name, const Variant &p_value);
    void set_restart_if_changed(const StringName &p_name, bool p_restart);
    void set_hide_from_editor(const StringName &p_name, bool p_hide);
    void set_ignore_value_in_docs(const StringName &p_name, bool p_ignore);
    bool get_ignore_value_in_docs(const StringName &p_name) const;
    bool property_can_revert(StringView p_name);
    Variant property_get_revert(StringView p_name);

    const String &get_project_data_dir_name() const;
    String get_project_data_path() const;
    const String &get_resource_path() const;

    static ProjectSettings *get_singleton();

    void clear(const StringName &p_name);
    int get_order(const StringName &p_name) const;
    void set_order(const StringName &p_name, int p_order);
    void set_builtin_order(const StringName &p_name);

    Error setup(StringView p_path, StringView p_main_pack, bool p_upwards = false, bool p_ignore_override = false);

    Error save_custom(StringView p_path = {}, const CustomMap &p_custom = CustomMap(), const Vector<String> &p_custom_features = {}, bool p_merge_with_current = true);
    Error save();
    void set_custom_property_info(const StringName &p_prop, const PropertyInfo &p_info);
    const HashMap<StringName, PropertyInfo> &get_custom_property_info() const;
    uint64_t get_last_saved_time() const { return last_save_time; }
    // used by some internal settings logic ( after props version upgrade )
    void set_last_saved_time(uint64_t save_time) { last_save_time = save_time; }

    Vector<String> get_optimizer_presets() const;

    const Vector<String> &get_input_presets() const { return input_presets; }

    void set_disable_feature_overrides(bool p_disable);

    bool is_using_datapack() const;

    void set_registering_order(bool p_enable);

    bool has_custom_feature(StringView p_feature) const;

    // Either use the signal `project_settings_changed` or query this function.
    // N.B. _dirty_this_frame is set initially to 2.
    // This is to cope with the situation where a project setting is changed in the iteration AFTER it is read.
    // There is therefore the potential for a change to be missed. Persisting the counter
    // for two frames avoids this, at the cost of a frame delay.
    bool has_changes() const { return _dirty_this_frame == 1; }
    void update();
    ProjectSettings();
    ~ProjectSettings() override;
};

//not a macro any longer
GODOT_EXPORT Variant _GLOBAL_DEF(const StringName &p_var, const Variant &p_default, bool p_restart_if_changed = false, bool p_ignore_value_in_docs = false);
template<class T>
T T_GLOBAL_DEF(const StringName& p_var, const T& p_default, bool p_restart_if_changed = false, bool p_ignore_value_in_docs = false) {
    return _GLOBAL_DEF(p_var,p_default,p_restart_if_changed,p_ignore_value_in_docs).template as<T>();
}
template<class T>
T T_GLOBAL_GET(const StringName& p_var) {
    return ProjectSettings::get_singleton()->getT<T>(p_var);
}

#define GLOBAL_DEF(m_var, m_value) _GLOBAL_DEF(m_var, m_value)
#define GLOBAL_DEF_RST(m_var, m_value) _GLOBAL_DEF(m_var, m_value, true)
#define GLOBAL_DEF_RST_NO_VAL(m_var, m_value) _GLOBAL_DEF(m_var, m_value, true)
#define GLOBAL_DEF_T_RST(m_var, m_value,type) T_GLOBAL_DEF<type>(m_var, m_value, true)
#define GLOBAL_GET(m_var) ProjectSettings::get_singleton()->get(m_var)

