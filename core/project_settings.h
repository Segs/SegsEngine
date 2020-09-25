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
class GODOT_EXPORT ProjectSettings : public Object {

    GDCLASS(ProjectSettings,Object)

    _THREAD_SAFE_CLASS_

public:
    using CustomMap = HashMap<StringName, Variant>;

    enum {
        //properties that are not for built in values begin from this value, so builtin ones are displayed first
        NO_BUILTIN_ORDER_BASE = 1 << 16
    };

protected:
    struct VariantContainer {
        Variant variant;
        Variant initial;
        int order=0;
        bool persist=false;
        bool hide_from_editor=false;
        bool overridden=false;
        bool restart_if_changed=false;
        VariantContainer() {}

        VariantContainer(const Variant &p_variant, int p_order, bool p_persist = false) :
                variant(p_variant),
                order(p_order),
                persist(p_persist)
        {
        }
    };

    bool registering_order;
    int last_order;
    int last_builtin_order;
    HashMap<StringName, VariantContainer> props;
    String resource_path;
    HashMap<StringName, PropertyInfo> custom_prop_info;
    bool disable_feature_overrides;
    bool using_datapack;
    List<String> input_presets;

    Set<String> custom_features;
    HashMap<StringName, StringName> feature_overrides;

    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(Vector<PropertyInfo> *p_list) const;

    static ProjectSettings *singleton;
public:
    Error _load_settings_text(StringView p_path);
    Error _load_settings_binary(StringView p_path);
    Error _load_settings_text_or_binary(StringView p_text_path, StringView p_bin_path);

    Error _save_settings_text(StringView p_file, const HashMap<String, List<String> > &props, const CustomMap &p_custom = CustomMap(), const String &p_custom_features = {});
    Error _save_settings_binary(StringView p_file, const HashMap<String, List<String> > &props, const CustomMap &p_custom = CustomMap(), const String &p_custom_features = {});

    Error _save_custom_bnd(StringView p_file);

    void _convert_to_last_version(int p_from_version);

    bool _load_resource_pack(StringView p_pack, bool p_replace_files = true);

    void _add_property_info_bind(const Dictionary &p_info);

    Error _setup(StringView p_path, StringView p_main_pack, bool p_upwards = false);

protected:
    static void _bind_methods();

public:
    static const int CONFIG_VERSION = 4;

    void set_setting(const StringName &p_setting, const Variant &p_value);
    Variant get_setting(const StringName &p_setting) const;

    bool has_setting(const StringName &p_var) const;
    String localize_path(StringView p_path) const;
    String globalize_path(StringView p_path) const;

    void set_initial_value(const StringName &p_name, const Variant &p_value);
    void set_restart_if_changed(const StringName &p_name, bool p_restart);
    bool property_can_revert(StringView p_name);
    Variant property_get_revert(StringView p_name);

    const String &get_resource_path() const;

    static ProjectSettings *get_singleton();

    void clear(const StringName &p_name);
    int get_order(const StringName &p_name) const;
    void set_order(const StringName &p_name, int p_order);
    void set_builtin_order(const StringName &p_name);

    Error setup(StringView p_path, StringView p_main_pack, bool p_upwards = false);

    Error save_custom(StringView p_path = {}, const CustomMap &p_custom = CustomMap(), const Vector<String> &p_custom_features = {}, bool p_merge_with_current = true);
    Error save();
    void set_custom_property_info(const StringName &p_prop, const PropertyInfo &p_info);
    const HashMap<StringName, PropertyInfo> &get_custom_property_info() const;

    Vector<String> get_optimizer_presets() const;

    const List<String> &get_input_presets() const { return input_presets; }

    void set_disable_feature_overrides(bool p_disable);

    bool is_using_datapack() const;

    void set_registering_order(bool p_enable);

    bool has_custom_feature(StringView p_feature) const;

    ProjectSettings();
    ~ProjectSettings() override;
};

//not a macro any longer
GODOT_EXPORT Variant _GLOBAL_DEF(const StringName &p_var, const Variant &p_default, bool p_restart_if_changed = false);
template<class T>
T T_GLOBAL_DEF(const StringName& p_var, const T& p_default, bool p_restart_if_changed = false) {
    return _GLOBAL_DEF(p_var,p_default,p_restart_if_changed).as<T>();
}
#define GLOBAL_DEF(m_var, m_value) _GLOBAL_DEF(m_var, m_value)
#define GLOBAL_T_DEF(m_var, m_value,m_type) T_GLOBAL_DEF<m_type>(m_var, m_value)
#define GLOBAL_DEF_RST(m_var, m_value) _GLOBAL_DEF(m_var, m_value, true)
#define GLOBAL_DEF_T_RST(m_var, m_value,type) T_GLOBAL_DEF<type>(m_var, m_value, true)
#define GLOBAL_GET(m_var) ProjectSettings::get_singleton()->get(m_var)
