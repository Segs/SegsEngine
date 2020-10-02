/*************************************************************************/
/*  nativescript.h                                                       */
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

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/method_info.h"
#include "core/oa_hash_map.h"
#include "core/ordered_hash_map.h"
#include "core/os/thread_safe.h"
#include "core/resource.h"
#include "core/script_language.h"
#include "core/self_list.h"
#include "core/hash_set.h"
#include "scene/main/node.h"

#include "modules/gdnative/gdnative.h"
#include <nativescript/godot_nativescript.h>

#include "core/os/mutex.h"

struct NativeScriptDesc {

    struct Method {
        godot_instance_method method;
        MethodInfo info;
        int rpc_mode;
        String documentation;
    };
    struct Property {
        godot_property_set_func setter;
        godot_property_get_func getter;
        PropertyInfo info;
        Variant default_value;
        int rset_mode;
        String documentation;
    };

    struct Signal {
        MethodInfo signal;
        String documentation;
    };

    Map<StringName, Method> methods;
    OrderedHashMap<StringName, Property> properties;
    Map<StringName, Signal> signals_; // QtCreator doesn't like the name signals
    StringName base;
    StringName base_native_type;
    NativeScriptDesc *base_data;
    godot_instance_create_func create_func;
    godot_instance_destroy_func destroy_func;

    String documentation;

    const void *type_tag;

    bool is_tool;

    inline NativeScriptDesc() :
            methods(),
            properties(),
            signals_(),
            base(),
            base_native_type(),
            documentation(),
            type_tag(nullptr) {
        memset(&create_func, 0, sizeof(godot_instance_create_func));
        memset(&destroy_func, 0, sizeof(godot_instance_destroy_func));
    }
};

class NativeScript : public Script {
    GDCLASS(NativeScript,Script)

#ifdef TOOLS_ENABLED
    HashSet<PlaceHolderScriptInstance *> placeholders;
    void _update_placeholder(PlaceHolderScriptInstance *p_placeholder);
    void _placeholder_erased(PlaceHolderScriptInstance *p_placeholder) override;
#endif

    friend class NativeScriptInstance;
    friend class NativeScriptLanguage;
    friend class NativeReloadNode;
    friend class GDNativeLibrary;

    Ref<GDNativeLibrary> library;

    String lib_path;

    StringName class_name;

    String script_class_name;
    String script_class_icon_path;

    Mutex *owners_lock;
    HashSet<Object *> instance_owners;

protected:
    static void _bind_methods();

public:
    inline NativeScriptDesc *get_script_desc() const;

    void set_class_name(String p_class_name);
    String get_class_name() const;

    void set_library(Ref<GDNativeLibrary> p_library);
    Ref<GDNativeLibrary> get_library() const;

    void set_script_class_name(String p_type);
    String get_script_class_name() const;
    void set_script_class_icon_path(String p_icon_path);
    String get_script_class_icon_path() const;

    bool can_instance() const override;

    Ref<Script> get_base_script() const override; //for script inheritance

    StringName get_instance_base_type() const override; // this may not work in all scripts, will return empty if so
    ScriptInstance *instance_create(Object *p_this) override;
    PlaceHolderScriptInstance *placeholder_instance_create(Object *p_this) override;
    bool instance_has(const Object *p_this) const override;

    bool has_source_code() const override;
    String get_source_code() const override;
    void set_source_code(const String &p_code) override;
    Error reload(bool p_keep_state = false) override;

    bool has_method(const StringName &p_method) const override;
    MethodInfo get_method_info(const StringName &p_method) const override;

    bool is_tool() const override;
    bool is_valid() const override;

    ScriptLanguage *get_language() const override;

    bool has_script_signal(const StringName &p_signal) const override;
    void get_script_signal_list(List<MethodInfo> *r_signals) const override;

    bool get_property_default_value(const StringName &p_property, Variant &r_value) const override;

    void update_exports() override; //editor tool
    void get_script_method_list(Vector<MethodInfo> *p_list) const override;
    void get_script_property_list(List<PropertyInfo> *p_list) const override;

    String get_class_documentation() const;
    String get_method_documentation(const StringName &p_method) const;
    String get_signal_documentation(const StringName &p_signal_name) const;
    String get_property_documentation(const StringName &p_path) const;

    Variant _new(const Variant **p_args, int p_argcount, Callable::CallError &r_error);

    NativeScript();
    ~NativeScript() override;
};

class NativeScriptInstance : public ScriptInstance {

    friend class NativeScript;

    Object *owner;
    Ref<NativeScript> script;
#ifdef DEBUG_ENABLED
    StringName current_method_call;
#endif

    void _ml_call_reversed(NativeScriptDesc *script_data, const StringName &p_method, const Variant **p_args, int p_argcount);

public:
    void *userdata;

    bool set(const StringName &p_name, const Variant &p_value) override;
    bool get(const StringName &p_name, Variant &r_ret) const override;
    void get_property_list(List<PropertyInfo> *p_properties) const override;
    VariantType get_property_type(const StringName &p_name, bool *r_is_valid) const override;
    void get_method_list(Vector<MethodInfo> *p_list) const override;
    bool has_method(const StringName &p_method) const override;
    Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;
    void notification(int p_notification) override;
    String to_string(bool *r_valid) override;
    Ref<Script> get_script() const override;
    MultiplayerAPI_RPCMode get_rpc_mode(const StringName &p_method) const override;
    MultiplayerAPI_RPCMode get_rset_mode(const StringName &p_variable) const override;
    ScriptLanguage *get_language() override;

    void call_multilevel(const StringName &p_method, const Variant **p_args, int p_argcount) override;
    void call_multilevel_reversed(const StringName &p_method, const Variant **p_args, int p_argcount) override;

    void refcount_incremented() override;
    bool refcount_decremented() override;

    ~NativeScriptInstance() override;
};

class NativeReloadNode;

class NativeScriptLanguage : public ScriptLanguage {

    friend class NativeScript;
    friend class NativeScriptInstance;
    friend class NativeReloadNode;

private:
    static NativeScriptLanguage *singleton;
    int lang_idx;

    void _unload_stuff(bool p_reload = false);

    Mutex *mutex;

    HashSet<Ref<GDNativeLibrary> > libs_to_init;
    HashSet<NativeScript *> scripts_to_register;
    volatile bool has_objects_to_register; // so that we don't lock mutex every frame - it's rarely needed
    void defer_init_library(Ref<GDNativeLibrary> lib, NativeScript *script);

    void init_library(const Ref<GDNativeLibrary> &lib);
    void register_script(NativeScript *script);
    void unregister_script(NativeScript *script);

    void call_libraries_cb(const StringName &name);

    Vector<Pair<bool, godot_instance_binding_functions> > binding_functions;
    Set<Vector<void *> *> binding_instances;

    Map<int, HashMap<StringName, const void *> > global_type_tags;

    struct ProfileData {
        StringName signature;
        uint64_t call_count;
        uint64_t self_time;
        uint64_t total_time;
        uint64_t frame_call_count;
        uint64_t frame_self_time;
        uint64_t frame_total_time;
        uint64_t last_frame_call_count;
        uint64_t last_frame_self_time;
        uint64_t last_frame_total_time;
    };

    Map<StringName, ProfileData> profile_data;
    bool profiling;

public:
    // These two maps must only be touched on the main thread
    Map<String, HashMap<StringName, NativeScriptDesc> > library_classes;
    Map<String, Ref<GDNative> > library_gdnatives;

    Map<String, HashSet<NativeScript *> > library_script_users;

    StringName _init_call_type;
    StringName _init_call_name;
    StringName _terminate_call_name;
    StringName _noarg_call_type;
    StringName _frame_call_name;
    StringName _thread_enter_call_name;
    StringName _thread_exit_call_name;

    NativeScriptLanguage();
    ~NativeScriptLanguage() override;

    inline static NativeScriptLanguage *get_singleton() {
        return singleton;
    }

    void _hacky_api_anchor();

    _FORCE_INLINE_ void set_language_index(int p_idx) { lang_idx = p_idx; }

    void thread_enter() override;
    void thread_exit() override;

    void frame() override;

    String get_name() const override;
    void init() override;
    String get_type() const override;
    String get_extension() const override;
    Error execute_file(StringView p_path) override;
    void finish() override;
    void get_reserved_words(List<String> *p_words) const override;
    void get_comment_delimiters(List<String> *p_delimiters) const override;
    void get_string_delimiters(List<String> *p_delimiters) const override;
    Ref<Script> get_template(StringView p_class_name, const String &p_base_class_name) const override;
    bool validate(const String &p_script, int &r_line_error, int &r_col_error, String &r_test_error, StringView p_path, List<String> *r_functions, List<ScriptLanguage::Warning> *r_warnings = nullptr, Set<int> *r_safe_lines = nullptr) const override;
    Script *create_script() const override;
    bool has_named_classes() const override;
    bool supports_builtin_mode() const override;
    int find_function(const String &p_function, const String &p_code) const override;
    String make_function(const String &p_class, const String &p_name, const PoolStringArray &p_args) const override;
    void auto_indent_code(String &p_code, int p_from_line, int p_to_line) const override;
    void add_global_constant(const StringName &p_variable, const Variant &p_value) override;
    const String &debug_get_error() const override;
    int debug_get_stack_level_count() const override;
    int debug_get_stack_level_line(int p_level) const override;
    String debug_get_stack_level_function(int p_level) const override;
    String debug_get_stack_level_source(int p_level) const override;
    void debug_get_stack_level_locals(int p_level, Vector<StringView> *p_locals, Vector<Variant> *p_values, int p_max_subitems, int p_max_depth) override;
    void debug_get_stack_level_members(int p_level, Vector<StringView> *p_members, Vector<Variant> *p_values, int p_max_subitems, int p_max_depth) override;
    void debug_get_globals(Vector<StringView> *p_locals, Vector<Variant> *p_values, int p_max_subitems, int p_max_depth) override;
    String debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) override;
    void reload_all_scripts() override;
    void reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) override;
    void get_recognized_extensions(List<String> *p_extensions) const override;
    void get_public_functions(List<MethodInfo> *p_functions) const override;
    void get_public_constants(List<Pair<String, Variant> > *p_constants) const override;
    void profiling_start() override;
    void profiling_stop() override;
    int profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) override;
    int profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) override;

    int register_binding_functions(godot_instance_binding_functions p_binding_functions);
    void unregister_binding_functions(int p_idx);

    void *get_instance_binding_data(int p_idx, Object *p_object);

    void *alloc_instance_binding_data(Object *p_object) override;
    void free_instance_binding_data(void *p_data) override;
    void refcount_incremented_instance_binding(Object *p_object) override;
    bool refcount_decremented_instance_binding(Object *p_object) override;

    void set_global_type_tag(int p_idx, StringName p_class_name, const void *p_type_tag);
    const void *get_global_type_tag(int p_idx, StringName p_class_name) const;

    bool handles_global_class_type(StringView p_type) const override;
    String get_global_class_name(StringView p_path, String *r_base_type, String *r_icon_path) const override;

    void profiling_add_data(StringName p_signature, uint64_t p_time);
};

inline NativeScriptDesc *NativeScript::get_script_desc() const {
    auto &classes(NativeScriptLanguage::singleton->library_classes[lib_path]);
    auto iter = classes.find(class_name);
    return iter!=classes.end() ? &iter->second : nullptr;
}

class NativeReloadNode : public Node {
    GDCLASS(NativeReloadNode,Node)

    bool unloaded;

public:
    static void _bind_methods();
    void _notification(int p_what);

    NativeReloadNode() :
            unloaded(false) {}
};

class ResourceFormatLoaderNativeScript : public ResourceFormatLoader {
public:
    RES load(StringView p_path, const String &p_original_path = String(), Error *r_error = nullptr) override;
    void get_recognized_extensions(Vector<String> &p_extensions) const override;
    bool handles_type(const String &p_type) const override;
    String get_resource_type(StringView p_path) const override;
};

class ResourceFormatSaverNativeScript : public ResourceFormatSaver {
    Error save(StringView p_path, const RES &p_resource, uint32_t p_flags = 0) override;
    bool recognize(const RES &p_resource) const override;
    void get_recognized_extensions(const RES &p_resource, Vector<String> *p_extensions) const override;
};
