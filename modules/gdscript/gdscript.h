/*************************************************************************/
/*  gdscript.h                                                           */
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

#include "core/io/resource_format_loader.h"
#include "core/io/resource_saver.h"
#include "core/script_language.h"
#include "gdscript_function.h"
#include "core/set.h"

namespace std {
class recursive_mutex;
}
using Mutex = std::recursive_mutex;

class GDScriptNativeClass : public RefCounted {

    GDCLASS(GDScriptNativeClass,RefCounted)

    StringName name;

protected:
    bool _get(const StringName &p_name, Variant &r_ret) const;
    static void _bind_methods();

public:
    _FORCE_INLINE_ const StringName &get_name() const { return name; }
    Variant _new();
    Object *instance();
    GDScriptNativeClass(const StringName &p_name);
};

class GDScript : public Script {

    GDCLASS(GDScript,Script)

    bool tool;
    bool valid;

    struct MemberInfo {
        int index;
        StringName setter;
        StringName getter;
        MultiplayerAPI_RPCMode rpc_mode;
        GDScriptDataType data_type;
    };

    friend class GDScriptInstance;
    friend class GDScriptFunction;
    friend class GDScriptCompiler;
    friend class GDScriptFunctions;
    friend class GDScriptLanguage;

    Variant _static_ref; //used for static call
    Ref<GDScriptNativeClass> native;
    Ref<GDScript> base;
    GDScript *_base; //fast pointer access
    GDScript *_owner; //for subclasses

    Set<StringName> members; //members are just indices to the instanced script.
    Map<StringName, Variant> constants;
    Map<StringName, GDScriptFunction *> member_functions;
    Map<StringName, MemberInfo> member_indices; //members are just indices to the instanced script.
    Map<StringName, Ref<GDScript> > subclasses;
    Map<StringName, Vector<StringName> > _signals;

#ifdef TOOLS_ENABLED

    Map<StringName, int> member_lines;

    Map<StringName, Variant> member_default_values;

    List<PropertyInfo> members_cache;
    Map<StringName, Variant> member_default_values_cache;
    Ref<GDScript> base_cache;
    Set<ObjectID> inheriters_cache;
    bool source_changed_cache;
    bool placeholder_fallback_enabled;
    void _update_exports_values(Map<StringName, Variant> &values, PODVector<PropertyInfo> &propnames);

#endif
    Map<StringName, PropertyInfo> member_info;

    GDScriptFunction *initializer; //direct pointer to _init , faster to locate

    int subclass_count;
    Set<Object *> instances;
    //exported members
    se_string source;
    se_string path;
    StringName name;
    SelfList<GDScript> script_list;

    GDScriptInstance *_create_instance(const Variant **p_args, int p_argcount, Object *p_owner, bool p_isref, Variant::CallError &r_error);

    void _set_subclass_path(Ref<GDScript> &p_sc, se_string_view p_path);

#ifdef TOOLS_ENABLED
    Set<PlaceHolderScriptInstance *> placeholders;
    //void _update_placeholder(PlaceHolderScriptInstance *p_placeholder);
    void _placeholder_erased(PlaceHolderScriptInstance *p_placeholder) override;
#endif

#ifdef DEBUG_ENABLED

    Map<ObjectID, List<Pair<StringName, Variant> > > pending_reload_state;

#endif

    bool _update_exports();

protected:
    bool _get(const StringName &p_name, Variant &r_ret) const;
    bool _set(const StringName &p_name, const Variant &p_value);
    void _get_property_list(ListPOD<PropertyInfo> *p_properties) const;

    Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error) override;
    //void call_multilevel(const StringName& p_method,const Variant** p_args,int p_argcount);

    static void _bind_methods();

public:
    bool is_valid() const override { return valid; }

    const Map<StringName, Ref<GDScript> > &get_subclasses() const { return subclasses; }
    const Map<StringName, Variant> &get_constants() const { return constants; }
    const Set<StringName> &get_members() const { return members; }
    const GDScriptDataType &get_member_type(const StringName &p_member) const {
        CRASH_COND(!member_indices.contains(p_member))
        return member_indices.at(p_member).data_type;
    }
    const Map<StringName, GDScriptFunction *> &get_member_functions() const { return member_functions; }
    const Ref<GDScriptNativeClass> &get_native() const { return native; }
    const StringName &get_script_class_name() const { return name; }

    bool has_script_signal(const StringName &p_signal) const override;
    void get_script_signal_list(ListPOD<MethodInfo> *r_signals) const override;

    bool is_tool() const override { return tool; }
    Ref<GDScript> get_base() const;

    const Map<StringName, MemberInfo> &debug_get_member_indices() const { return member_indices; }
    const Map<StringName, GDScriptFunction *> &debug_get_member_functions() const; //this is debug only
    StringName debug_get_member_by_index(int p_idx) const;

    Variant _new(const Variant **p_args, int p_argcount, Variant::CallError &r_error);
    bool can_instance() const override;

    Ref<Script> get_base_script() const override;

    StringName get_instance_base_type() const override; // this may not work in all scripts, will return empty if so
    ScriptInstance *instance_create(Object *p_this) override;
    PlaceHolderScriptInstance *placeholder_instance_create(Object *p_this) override;
    bool instance_has(const Object *p_this) const override;

    bool has_source_code() const override;
    se_string_view get_source_code() const override;
    void set_source_code(se_string p_code) override;
    void update_exports() override;

    Error reload(bool p_keep_state = false) override;

    void set_script_path(se_string_view p_path) { path = p_path; } //because subclasses need a path too...
    Error load_source_code(se_string_view p_path);
    Error load_byte_code(se_string_view p_path);

    Vector<uint8_t> get_as_byte_code() const;

    bool get_property_default_value(const StringName &p_property, Variant &r_value) const override;

    void get_script_method_list(PODVector<MethodInfo> *p_list) const override;
    bool has_method(const StringName &p_method) const override;
    MethodInfo get_method_info(const StringName &p_method) const override;

    void get_script_property_list(ListPOD<PropertyInfo> *p_list) const override;

    ScriptLanguage *get_language() const override;

    int get_member_line(const StringName &p_member) const override {
#ifdef TOOLS_ENABLED
        if (member_lines.contains(p_member))
            return member_lines.at(p_member);
        else
#endif
            return -1;
    }

    void get_constants(Map<StringName, Variant> *p_constants) override;
    void get_members(Set<StringName> *p_members) override;

#ifdef TOOLS_ENABLED
    bool is_placeholder_fallback_enabled() const override { return placeholder_fallback_enabled; }
#endif

    GDScript();
    ~GDScript() override;
};

class GDScriptInstance : public ScriptInstance {
    friend class GDScript;
    friend class GDScriptFunction;
    friend class GDScriptFunctions;
    friend class GDScriptCompiler;

    Object *owner;
    Ref<GDScript> script;
#ifdef DEBUG_ENABLED
    Map<StringName, int> member_indices_cache; //used only for hot script reloading
#endif
    Vector<Variant> members;
    bool base_ref;

    void _ml_call_reversed(GDScript *sptr, const StringName &p_method, const Variant **p_args, int p_argcount);

public:
    Object *get_owner() override { return owner; }

    bool set(const StringName &p_name, const Variant &p_value) override;
    bool get(const StringName &p_name, Variant &r_ret) const override;
    void get_property_list(ListPOD<PropertyInfo> *p_properties) const override;
    VariantType get_property_type(const StringName &p_name, bool *r_is_valid = nullptr) const override;

    void get_method_list(PODVector<MethodInfo> *p_list) const override;
    bool has_method(const StringName &p_method) const override;
    Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error) override;
    void call_multilevel(const StringName &p_method, const Variant **p_args, int p_argcount) override;
    void call_multilevel_reversed(const StringName &p_method, const Variant **p_args, int p_argcount) override;

    Variant debug_get_member_by_index(int p_idx) const { return members[p_idx]; }

    void notification(int p_notification) override;
    se_string to_string(bool *r_valid) override;

    Ref<Script> get_script() const override;

    ScriptLanguage *get_language() override;

    void set_path(se_string_view p_path);

    void reload_members();

    MultiplayerAPI_RPCMode get_rpc_mode(const StringName &p_method) const override;
    MultiplayerAPI_RPCMode get_rset_mode(const StringName &p_variable) const override;

    GDScriptInstance();
    ~GDScriptInstance() override;
};

#ifdef DEBUG_ENABLED
struct GDScriptWarning {
    enum Code {
        UNASSIGNED_VARIABLE, // Variable used but never assigned
        UNASSIGNED_VARIABLE_OP_ASSIGN, // Variable never assigned but used in an assignment operation (+=, *=, etc)
        UNUSED_VARIABLE, // Local variable is declared but never used
        SHADOWED_VARIABLE, // Variable name shadowed by other variable
        UNUSED_CLASS_VARIABLE, // Class variable is declared but never used in the file
        UNUSED_ARGUMENT, // Function argument is never used
        UNREACHABLE_CODE, // Code after a return statement
        STANDALONE_EXPRESSION, // Expression not assigned to a variable
        VOID_ASSIGNMENT, // Function returns void but it's assigned to a variable
        NARROWING_CONVERSION, // Float value into an integer slot, precision is lost
        FUNCTION_MAY_YIELD, // Typed assign of function call that yields (it may return a function state)
        VARIABLE_CONFLICTS_FUNCTION, // Variable has the same name of a function
        FUNCTION_CONFLICTS_VARIABLE, // Function has the same name of a variable
        FUNCTION_CONFLICTS_CONSTANT, // Function has the same name of a constant
        INCOMPATIBLE_TERNARY, // Possible values of a ternary if are not mutually compatible
        UNUSED_SIGNAL, // Signal is defined but never emitted
        RETURN_VALUE_DISCARDED, // Function call returns something but the value isn't used
        PROPERTY_USED_AS_FUNCTION, // Function not found, but there's a property with the same name
        CONSTANT_USED_AS_FUNCTION, // Function not found, but there's a constant with the same name
        FUNCTION_USED_AS_PROPERTY, // Property not found, but there's a function with the same name
        INTEGER_DIVISION, // Integer divide by integer, decimal part is discarded
        UNSAFE_PROPERTY_ACCESS, // Property not found in the detected type (but can be in subtypes)
        UNSAFE_METHOD_ACCESS, // Function not found in the detected type (but can be in subtypes)
        UNSAFE_CAST, // Cast used in an unknown type
        UNSAFE_CALL_ARGUMENT, // Function call argument is of a supertype of the require argument
        DEPRECATED_KEYWORD, // The keyword is deprecated and should be replaced
        WARNING_MAX,
    } code;
    Vector<se_string> symbols;
    int line;

    se_string get_name() const;
    se_string get_message() const;
    static const char *get_name_from_code(Code p_code);
    static Code get_code_from_name(const se_string &p_name);

    GDScriptWarning() :
            code(WARNING_MAX),
            line(-1) {}
};
#endif // DEBUG_ENABLED

class GDScriptLanguage : public ScriptLanguage {

    static GDScriptLanguage *singleton;

    Variant *_global_array;
    Vector<Variant> global_array;
    Map<StringName, int> globals;
    Map<StringName, Variant> named_globals;

    struct CallLevel {

        Variant *stack;
        GDScriptFunction *function;
        GDScriptInstance *instance;
        int *ip;
        int *line;
    };

    int _debug_parse_err_line;
    se_string _debug_parse_err_file;
    se_string _debug_error;
    int _debug_call_stack_pos;
    int _debug_max_call_stack;
    CallLevel *_call_stack;

    void _add_global(const StringName &p_name, const Variant &p_value);

    friend class GDScriptInstance;

    Mutex *lock;

    friend class GDScript;

    SelfList<GDScript>::List script_list;
    friend class GDScriptFunction;

    SelfList<GDScriptFunction>::List function_list;
    bool profiling;
    uint64_t script_frame_time;

public:
    int calls;

    bool debug_break(se_string_view p_error, bool p_allow_continue = true);
    bool debug_break_parse(se_string_view p_file, int p_line, se_string_view p_error);

    _FORCE_INLINE_ void enter_function(GDScriptInstance *p_instance, GDScriptFunction *p_function, Variant *p_stack, int *p_ip, int *p_line) {

        if (Thread::get_main_id() != Thread::get_caller_id())
            return; //no support for other threads than main for now

        if (ScriptDebugger::get_singleton()->get_lines_left() > 0 && ScriptDebugger::get_singleton()->get_depth() >= 0)
            ScriptDebugger::get_singleton()->set_depth(ScriptDebugger::get_singleton()->get_depth() + 1);

        if (_debug_call_stack_pos >= _debug_max_call_stack) {
            //stack overflow
            _debug_error = "Stack Overflow (Stack Size: " + ::to_string(_debug_max_call_stack) + ")";
            ScriptDebugger::get_singleton()->debug(this);
            return;
        }

        _call_stack[_debug_call_stack_pos].stack = p_stack;
        _call_stack[_debug_call_stack_pos].instance = p_instance;
        _call_stack[_debug_call_stack_pos].function = p_function;
        _call_stack[_debug_call_stack_pos].ip = p_ip;
        _call_stack[_debug_call_stack_pos].line = p_line;
        _debug_call_stack_pos++;
    }

    _FORCE_INLINE_ void exit_function() {

        if (Thread::get_main_id() != Thread::get_caller_id())
            return; //no support for other threads than main for now

        if (ScriptDebugger::get_singleton()->get_lines_left() > 0 && ScriptDebugger::get_singleton()->get_depth() >= 0)
            ScriptDebugger::get_singleton()->set_depth(ScriptDebugger::get_singleton()->get_depth() - 1);

        if (_debug_call_stack_pos == 0) {

            _debug_error = "Stack Underflow (Engine Bug)";
            ScriptDebugger::get_singleton()->debug(this);
            return;
        }

        _debug_call_stack_pos--;
    }

    Vector<StackInfo> debug_get_current_stack_info() override {
        if (Thread::get_main_id() != Thread::get_caller_id())
            return Vector<StackInfo>();

        Vector<StackInfo> csi;
        csi.resize(_debug_call_stack_pos);
        for (int i = 0; i < _debug_call_stack_pos; i++) {
            csi.write[_debug_call_stack_pos - i - 1].line = _call_stack[i].line ? *_call_stack[i].line : 0;
            if (_call_stack[i].function) {
                csi.write[_debug_call_stack_pos - i - 1].func = _call_stack[i].function->get_name().asCString();
                csi.write[_debug_call_stack_pos - i - 1].file = _call_stack[i].function->get_script()->get_path();
            }
        }
        return csi;
    }

    struct {

        StringName _init;
        StringName _notification;
        StringName _set;
        StringName _get;
        StringName _get_property_list;
        StringName _script_source;

    } strings;

    _FORCE_INLINE_ int get_global_array_size() const { return global_array.size(); }
    _FORCE_INLINE_ Variant *get_global_array() { return _global_array; }
    _FORCE_INLINE_ const Map<StringName, int> &get_global_map() const { return globals; }
    _FORCE_INLINE_ const Map<StringName, Variant> &get_named_globals_map() const { return named_globals; }

    _FORCE_INLINE_ static GDScriptLanguage *get_singleton() { return singleton; }

    StringName get_name() const override;

    /* LANGUAGE FUNCTIONS */
    void init() override;
    se_string get_type() const override;
    se_string get_extension() const override;
    Error execute_file(se_string_view p_path) override;
    void finish() override;

    /* EDITOR FUNCTIONS */
    void get_reserved_words(List<se_string> *p_words) const override;
    void get_comment_delimiters(List<se_string> *p_delimiters) const override;
    void get_string_delimiters(List<se_string> *p_delimiters) const override;
    virtual se_string _get_processed_template(se_string_view p_template, se_string_view p_base_class_name) const;
    Ref<Script> get_template(se_string_view p_class_name, se_string_view p_base_class_name) const override;
    bool is_using_templates() override;
    void make_template(se_string_view p_class_name, se_string_view p_base_class_name, Ref<Script> &p_script) override;
    bool validate(se_string_view p_script, int &r_line_error, int &r_col_error, se_string &r_test_error, se_string_view p_path = {}, List<se_string> *r_functions = nullptr, List<ScriptLanguage::Warning> *r_warnings = nullptr, Set<int> *r_safe_lines = nullptr) const override;
    Script *create_script() const override;
    bool has_named_classes() const override;
    bool supports_builtin_mode() const override;
    bool can_inherit_from_file() override { return true; }
    int find_function(se_string_view p_function, se_string_view p_code) const override;
    se_string make_function(const se_string &p_class, const StringName &p_name, const PoolVector<se_string> &p_args) const override;
    Error complete_code(const se_string &p_code, se_string_view p_path, Object *p_owner, List<ScriptCodeCompletionOption> *r_options, bool &r_forced, se_string &r_call_hint) override;
#ifdef TOOLS_ENABLED
    Error lookup_code(se_string_view p_code, se_string_view p_symbol, se_string_view p_path, Object *p_owner, LookupResult &r_result) override;
#endif
    virtual const char *_get_indentation() const;
    void auto_indent_code(se_string &p_code, int p_from_line, int p_to_line) const override;
    void add_global_constant(const StringName &p_variable, const Variant &p_value) override;
    void add_named_global_constant(const StringName &p_name, const Variant &p_value) override;
    void remove_named_global_constant(const StringName &p_name) override;

    /* DEBUGGER FUNCTIONS */

    const se_string &debug_get_error() const override;
    int debug_get_stack_level_count() const override;
    int debug_get_stack_level_line(int p_level) const override;
    se_string debug_get_stack_level_function(int p_level) const override;
    se_string debug_get_stack_level_source(int p_level) const override;
    void debug_get_stack_level_locals(int p_level, ListPOD<se_string> *p_locals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override;
    void debug_get_stack_level_members(int p_level, ListPOD<se_string> *p_members, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override;
    ScriptInstance *debug_get_stack_level_instance(int p_level) override;
    void debug_get_globals(ListPOD<se_string> *p_globals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override;
    se_string debug_parse_stack_level_expression(int p_level, se_string_view p_expression, int p_max_subitems = -1, int p_max_depth = -1) override;

    void reload_all_scripts() override;
    void reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) override;

    void frame() override;

    void get_public_functions(List<MethodInfo> *p_functions) const override;
    void get_public_constants(List<Pair<se_string_view, Variant> > *p_constants) const override;

    void profiling_start() override;
    void profiling_stop() override;

    int profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) override;
    int profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) override;

    /* LOADER FUNCTIONS */

    void get_recognized_extensions(List<se_string> *p_extensions) const override;

    /* GLOBAL CLASSES */

    bool handles_global_class_type(se_string_view p_type) const override;
    StringName get_global_class_name(se_string_view p_path, se_string *r_base_type = nullptr, se_string *r_icon_path = nullptr) const override;

    GDScriptLanguage();
    ~GDScriptLanguage() override;
};

class ResourceFormatLoaderGDScript : public ResourceFormatLoader {
public:
    RES load(se_string_view p_path, se_string_view p_original_path = se_string_view(), Error *r_error = nullptr) override;
    void get_recognized_extensions(PODVector<se_string> &p_extensions) const override;
    bool handles_type(se_string_view p_type) const override;
    se_string get_resource_type(se_string_view p_path) const override;
    void get_dependencies(se_string_view p_path, ListPOD<se_string> *p_dependencies, bool p_add_types = false) override;
};

class ResourceFormatSaverGDScript : public ResourceFormatSaver {
public:
    Error save(se_string_view p_path, const RES &p_resource, uint32_t p_flags = 0) override;
    void get_recognized_extensions(const RES &p_resource, PODVector<se_string> &p_extensions) const override;
    bool recognize(const RES &p_resource) const override;
};

