/*************************************************************************/
/*  csharp_script.cpp                                                    */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "csharp_script.h"

#include <mono/metadata/threads.h>

#include "core/debugger/script_debugger.h"
#include "core/io/json.h"
#include "core/method_bind_interface.h"
#include "core/method_bind.h"
#include "core/object_db.h"
#include "core/os/file_access.h"
#include "core/os/keyboard.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/project_settings.h"
#include "core/property_info.h"
#include "core/io/multiplayer_api.h"
#include "core/translation_helpers.h"
#include "core/print_string.h"

#ifdef TOOLS_ENABLED
//#include "editor/bindings_generator.h"
#include "editor/csharp_project.h"
#include "editor/editor_node.h"
#include "editor/node_dock.h"
#endif

#include "editor/editor_internal_calls.h"
#include "godotsharp_dirs.h"
#include "managed_callable.h"
#include "mono_gd/gd_mono_cache.h"
#include "mono_gd/gd_mono_class.h"
#include "mono_gd/gd_mono_marshal.h"
#include "mono_gd/gd_mono_utils.h"
#include "signal_awaiter_utils.h"
#include "core/reflection_support/reflection_data.h"
#include "utils/macros.h"
#include "utils/string_utils.h"
#include "EASTL/sort.h"
#include "editor/editor_settings.h"

#define CACHED_STRING_NAME(m_var) (CSharpLanguage::get_singleton()->get_string_names().m_var)

IMPL_GDCLASS(CSharpScript)
IMPL_GDCLASS(ManagedCallableMiddleman)

#ifdef TOOLS_ENABLED
static bool _create_project_solution_if_needed() {

    String sln_path = GodotSharpDirs::get_project_sln_path();
    String csproj_path = GodotSharpDirs::get_project_csproj_path();

    if (!FileAccess::exists(sln_path) || !FileAccess::exists(csproj_path)) {
        // A solution does not yet exist, create a new one

        CRASH_COND(CSharpLanguage::get_singleton()->get_godotsharp_editor() == nullptr);
        return CSharpLanguage::get_singleton()->get_godotsharp_editor()->call_va("CreateProjectSolution").as<bool>();
    }

    return true;
}
#endif

CSharpLanguage *CSharpLanguage::singleton = nullptr;

StringName CSharpLanguage::get_name() const {

    return "C#";
}

String CSharpLanguage::get_type() const {

    return "CSharpScript";
}

String CSharpLanguage::get_extension() const {

    return "cs";
}

Error CSharpLanguage::execute_file(StringView p_path) {

    // ??
    return OK;
}

bool CSharpLanguage::init() {

    gdmono = memnew(GDMono);
    gdmono->initialize();

    if(!gdmono->is_runtime_initialized()) {
        ERR_PRINT("Cannot initialize CSharpLanguage: runtime is not initialized");
        return false;
    }
    if(!gdmono->initialize_load_assemblies()) {
        return false;
    }

#ifdef TOOLS_ENABLED
    EditorNode::add_init_callback(&_editor_init_callback);
#endif
    return true;
}
void CSharpLanguage::finish() {
    finalize();
}

void CSharpLanguage::finalize() {
    if (finalized) {
        return;
    }

    finalizing = true;

    // Make sure all script binding gchandles are released before finalizing GDMono
    for (auto &E : script_bindings) {
        CSharpScriptBinding &script_binding = E.second;

        if (!script_binding.gchandle.is_released()) {
            script_binding.gchandle.release();
            script_binding.inited = false;
        }
    }

    memdelete(gdmono);
    gdmono = nullptr;

    // Clear here, after finalizing all domains to make sure there is nothing else referencing the elements.
    script_bindings.clear();
#ifdef DEBUG_ENABLED
    for (auto &E : unsafe_object_references) {
        const GameEntity id = E.first;
        Object *obj = object_for_entity(id);

        if (obj) {
            ERR_PRINT(String("Leaked unsafe reference to object: ") + obj->to_string());
        } else {
            ERR_PRINT("Leaked unsafe reference to deleted object: " + itos(entt::to_integral(id)));
        }
    }
#endif
    memdelete(managed_callable_middleman);
    finalizing = false;
    finalized = true;
}

void CSharpLanguage::get_reserved_words(Vector<String> *p_words) const {

    static const char *_reserved_words[] = {
        // Reserved keywords
        "abstract",
        "as",
        "base",
        "bool",
        "break",
        "byte",
        "case",
        "catch",
        "char",
        "checked",
        "class",
        "const",
        "continue",
        "decimal",
        "default",
        "delegate",
        "do",
        "double",
        "else",
        "enum",
        "event",
        "explicit",
        "extern",
        "false",
        "finally",
        "fixed",
        "float",
        "for",
        "foreach",
        "goto",
        "if",
        "implicit",
        "in",
        "int",
        "interface",
        "internal",
        "is",
        "lock",
        "long",
        "namespace",
        "new",
        "null",
        "object",
        "operator",
        "out",
        "override",
        "params",
        "private",
        "protected",
        "public",
        "readonly",
        "ref",
        "return",
        "sbyte",
        "sealed",
        "short",
        "sizeof",
        "stackalloc",
        "static",
        "string",
        "struct",
        "switch",
        "this",
        "throw",
        "true",
        "try",
        "typeof",
        "uint",
        "ulong",
        "unchecked",
        "unsafe",
        "ushort",
        "using",
        "virtual",
        "void",
        "volatile",
        "while",

        // Contextual keywords. Not reserved words, but I guess we should include
        // them because this seems to be used only for syntax highlighting.
        "add",
        "alias",
        "ascending",
        "async",
        "await",
        "by",
        "descending",
        "dynamic",
        "equals",
        "from",
        "get",
        "global",
        "group",
        "into",
        "join",
        "let",
        "nameof",
        "on",
        "orderby",
        "partial",
        "remove",
        "select",
        "set",
        "value",
        "var",
        "when",
        "where",
        "yield",
    };
    p_words->assign(eastl::begin(_reserved_words),eastl::end(_reserved_words));
}

bool CSharpLanguage::is_control_flow_keyword(String p_keyword) const
{
    return p_keyword == "break" ||
           p_keyword == "case" ||
           p_keyword == "catch" ||
           p_keyword == "continue" ||
           p_keyword == "default" ||
           p_keyword == "do" ||
           p_keyword == "else" ||
           p_keyword == "finally" ||
           p_keyword == "for" ||
           p_keyword == "foreach" ||
           p_keyword == "goto" ||
           p_keyword == "if" ||
           p_keyword == "return" ||
           p_keyword == "switch" ||
           p_keyword == "throw" ||
           p_keyword == "try" ||
           p_keyword == "while";
}

void CSharpLanguage::get_comment_delimiters(Vector<String> *p_delimiters) const {

    p_delimiters->push_back("//"); // single-line comment
    p_delimiters->push_back("/* */"); // delimited comment
}

void CSharpLanguage::get_string_delimiters(Vector<String> *p_delimiters) const {

    p_delimiters->push_back("' '"); // character literal
    p_delimiters->push_back("\" \""); // regular string literal
    // Verbatim string literals (`@" "`) don't render correctly, so don't highlight them.
    // Generic string highlighting suffices as a workaround for now.

}

static String get_base_class_name(StringView p_base_class_name, StringView p_class_name) {

    String base_class(p_base_class_name);
    if (p_class_name == StringView(base_class)) {
        base_class = "Godot." + base_class;
    }
    return base_class;
}

Ref<Script> CSharpLanguage::get_template(StringView p_class_name, StringView p_base_class_name) const {

    String script_template = "using " BINDINGS_NAMESPACE ";\n"
                             "using System;\n"
                             "\n"
                             "public class %CLASS% : %BASE%\n"
                             "{\n"
                             "    // Declare member variables here. Examples:\n"
                             "    // private int a = 2;\n"
                             "    // private string b = \"text\";\n"
                             "\n"
                             "    // Called when the node enters the scene tree for the first time.\n"
                             "    public override void _Ready()\n"
                             "    {\n"
                             "        \n"
                             "    }\n"
                             "\n"
                             "//  // Called every frame. 'delta' is the elapsed time since the previous frame.\n"
                             "//  public override void _Process(float delta)\n"
                             "//  {\n"
                             "//      \n"
                             "//  }\n"
                             "}\n";
    // Replaces all spaces in p_class_name with underscores to prevent
    // invalid C# Script templates from being generated when the object name
    // has spaces in it.
    String class_name_no_spaces = StringUtils::replace(p_class_name," ", "_");
    String base_class_name = get_base_class_name(p_base_class_name, class_name_no_spaces);

    script_template = script_template.replaced("%BASE%", base_class_name)
                              .replaced("%CLASS%", class_name_no_spaces);

    Ref<CSharpScript> script=make_ref_counted<CSharpScript>();
    script->set_source_code(script_template);
    script->set_name(class_name_no_spaces);

    return script;
}

bool CSharpLanguage::is_using_templates() {

    return true;
}

void CSharpLanguage::make_template(StringView p_class_name, StringView p_base_class_name, const Ref<Script> &p_script) {

    String src(p_script->get_source_code());
    String class_name_no_spaces = StringUtils::replace(p_class_name," ", "_");
    String base_class_name = get_base_class_name(p_base_class_name, class_name_no_spaces);

    src = src.replaced("%BASE%", base_class_name)
              .replaced("%CLASS%", class_name_no_spaces)
              .replaced("%TS%", _get_indentation());
    p_script->set_source_code(src);
}
/* TODO */
bool CSharpLanguage::validate(StringView p_script, int &r_line_error, int &r_col_error, String &r_test_error,
        StringView p_path, Vector<String> *r_functions, Vector<ScriptLanguage::Warning> *r_warnings,
        Set<int> *r_safe_lines) const {
    return true;
}
#ifdef TOOLS_ENABLED

String CSharpLanguage::validate_path(StringView p_path) const {

    String class_name(PathUtils::get_basename(PathUtils::get_file(p_path)));
    Vector<String> keywords;
    get_reserved_words(&keywords);
    if (keywords.contains(class_name)) {
        return TTR("Class name can't be a reserved keyword").asCString();
    }
    return "";
}
#endif
Script *CSharpLanguage::create_script() const {

    return memnew(CSharpScript);
}

bool CSharpLanguage::has_named_classes() const {

    return false;
}

bool CSharpLanguage::supports_builtin_mode() const {

    return false;
}

#ifdef TOOLS_ENABLED
static StringName variant_type_to_managed_name(StringName p_var_type_name) {

    if (p_var_type_name.empty()) {
        return "object";
    }

    if (!ClassDB::class_exists(p_var_type_name)) {
        return p_var_type_name;
    }

    if (p_var_type_name == Variant::get_type_name(VariantType::OBJECT)) {
        return "Godot.Object";
    }

    if (p_var_type_name == Variant::get_type_name(VariantType::FLOAT)) {
#ifdef REAL_T_IS_DOUBLE
        return "double";
#else
        return "float";
#endif
    }

    if (p_var_type_name == Variant::get_type_name(VariantType::STRING)) {
        return "string"; // I prefer this one >:[
    }

    if (p_var_type_name == Variant::get_type_name(VariantType::DICTIONARY))
        return "Collections.Dictionary";

    if (p_var_type_name == Variant::get_type_name(VariantType::ARRAY))
        return "Collections.Array";

    if (p_var_type_name == Variant::get_type_name(VariantType::POOL_BYTE_ARRAY))
        return "byte[]";
    if (p_var_type_name == Variant::get_type_name(VariantType::POOL_INT_ARRAY))
        return "int[]";
    if (p_var_type_name == Variant::get_type_name(VariantType::POOL_FLOAT32_ARRAY)) {
#ifdef REAL_T_IS_DOUBLE
        return "double[]";
#else
        return "float[]";
#endif
    }
    if (p_var_type_name == Variant::get_type_name(VariantType::POOL_STRING_ARRAY))
        return "string[]";
    if (p_var_type_name == Variant::get_type_name(VariantType::POOL_VECTOR2_ARRAY))
        return "Vector2[]";
    if (p_var_type_name == Variant::get_type_name(VariantType::POOL_VECTOR3_ARRAY))
        return "Vector3[]";
    if (p_var_type_name == Variant::get_type_name(VariantType::POOL_COLOR_ARRAY))
        return "Color[]";
    if (p_var_type_name == Variant::get_type_name(VariantType::SIGNAL)) {
        return "SignalInfo";
    }
    VariantType var_types[] = {
        VariantType::BOOL,
        VariantType::INT,
        VariantType::VECTOR2,
        VariantType::RECT2,
        VariantType::VECTOR3,
        VariantType::TRANSFORM2D,
        VariantType::PLANE,
        VariantType::QUAT,
        VariantType::AABB,
        VariantType::BASIS,
        VariantType::TRANSFORM,
        VariantType::COLOR,
        VariantType::STRING_NAME,
        VariantType::NODE_PATH,
        VariantType::_RID,
        VariantType::CALLABLE
    };

    for (unsigned int i = 0; i < sizeof(var_types) / sizeof(VariantType); i++) {
        if (p_var_type_name == Variant::get_type_name(var_types[i]))
            return p_var_type_name;
    }

    return "object";
}

String CSharpLanguage::make_function(const String &, const StringName &p_name, const PoolVector<String> &p_args) const {
    // FIXME
    // - Due to Godot's API limitation this just appends the function to the end of the file
    // - Use fully qualified name if there is ambiguity
    String s = "private void " + String(p_name) + "(";
    for (int i = 0; i < p_args.size(); i++) {
        const String &arg = p_args[i];

        if (i > 0) {
            s += ", ";
        }

        s += String(variant_type_to_managed_name(StringName(StringUtils::get_slice(arg, ':', 1)))) + " " +
             escape_csharp_keyword(StringUtils::get_slice(arg, ":", 0));
    }
    s += ")\n{\n    // Replace with function body.\n}\n";

    return s;
}
#else
String CSharpLanguage::make_function(const String &, const StringName &/*p_name*/, const PoolVector<String> &/*p_args*/) const {
    return String();
}
#endif

String CSharpLanguage::_get_indentation() const {
#ifdef TOOLS_ENABLED
    if (Engine::get_singleton()->is_editor_hint()) {
        bool use_space_indentation = EDITOR_GET_T<bool>("text_editor/indent/type");

        if (use_space_indentation) {
            int indent_size = EDITOR_GET_T<int>("text_editor/indent/size");

            String space_indent(indent_size,' ');
            return space_indent;
        }
    }
#endif
    return "\t";
}

const String &CSharpLanguage::debug_get_error() const {

    return _debug_error;
}

int CSharpLanguage::debug_get_stack_level_count() const {

    if (_debug_parse_err_line >= 0) {
        return 1;
    }

    // TODO: StackTrace
    return 1;
}

int CSharpLanguage::debug_get_stack_level_line(int p_level) const {

    if (_debug_parse_err_line >= 0) {
        return _debug_parse_err_line;
    }

    // TODO: StackTrace
    return 1;
}

String CSharpLanguage::debug_get_stack_level_function(int p_level) const {

    if (_debug_parse_err_line >= 0) {
        return String();
    }

    // TODO: StackTrace
    return String();
}

String CSharpLanguage::debug_get_stack_level_source(int p_level) const {

    if (_debug_parse_err_line >= 0) {
        return _debug_parse_err_file;
    }

    // TODO: StackTrace
    return String();
}

Vector<ScriptLanguage::StackInfo> CSharpLanguage::debug_get_current_stack_info() {

#ifdef DEBUG_ENABLED
    // Printing an error here will result in endless recursion, so we must be careful
    static thread_local bool _recursion_flag_ = false;
    if (_recursion_flag_) {
        return {};
    }
    _recursion_flag_ = true;
    SCOPE_EXIT { _recursion_flag_ = false; };

    GD_MONO_SCOPE_THREAD_ATTACH;

    if (!gdmono->is_runtime_initialized() || !GDMono::get_singleton()->get_core_api_assembly() || !GDMonoCache::cached_data.corlib_cache_updated) {
        return Vector<StackInfo>();
    }

    MonoObject *stack_trace = mono_object_new(mono_domain_get(), CACHED_CLASS(System_Diagnostics_StackTrace)->get_mono_ptr());

    MonoBoolean need_file_info = true;
    void *ctor_args[1] = { &need_file_info };

    CACHED_METHOD(System_Diagnostics_StackTrace, ctor_bool)->invoke_raw(stack_trace, ctor_args);

    Vector<StackInfo> si(stack_trace_get_info(stack_trace));

    return si;
#else
    return {};
#endif
}

#ifdef DEBUG_ENABLED
Vector<ScriptLanguage::StackInfo> CSharpLanguage::stack_trace_get_info(MonoObject *p_stack_trace) {

    // Printing an error here will result in endless recursion, so we must be careful
    static thread_local bool _recursion_flag_ = false;
    if (_recursion_flag_) {
        return {};
    }
    _recursion_flag_ = true;
    SCOPE_EXIT { _recursion_flag_ = false; };

    GD_MONO_SCOPE_THREAD_ATTACH;

    MonoException *exc = nullptr;

    MonoArray *frames = CACHED_METHOD_THUNK(System_Diagnostics_StackTrace, GetFrames).invoke(p_stack_trace, &exc);

    if (exc) {
        GDMonoUtils::debug_print_unhandled_exception(exc);
        return {};
    }

    int frame_count = mono_array_length(frames);

    if (frame_count <= 0)
        return {};

    Vector<StackInfo> si;
    si.resize(frame_count);

    for (int i = 0; i < frame_count; i++) {
        StackInfo &sif = si[i];
        MonoObject *frame = mono_array_get(frames, MonoObject *, i);

        MonoString *file_name;
        int file_line_num;
        MonoString *method_decl;
        CACHED_METHOD_THUNK(DebuggingUtils, GetStackFrameInfo).invoke(frame, &file_name, &file_line_num, &method_decl, &exc);

        if (exc) {
            GDMonoUtils::debug_print_unhandled_exception(exc);
            return Vector<StackInfo>();
        }

        // TODO
        // what if the StackFrame method is null (method_decl is empty). should we skip this frame?
        // can reproduce with a MissingMethodException on internal calls

        sif.file = GDMonoMarshal::mono_string_to_godot(file_name);
        sif.line = file_line_num;
        sif.func = GDMonoMarshal::mono_string_to_godot(method_decl);
    }

    return si;
}
#endif

void CSharpLanguage::post_unsafe_reference(Object *p_obj) {
#ifdef DEBUG_ENABLED
    MutexLock lock(unsafe_object_references_lock);
    GameEntity id = p_obj->get_instance_id();
    unsafe_object_references[id]++;
#endif
}

void CSharpLanguage::pre_unsafe_unreference(Object *p_obj) {
#ifdef DEBUG_ENABLED
    MutexLock lock(unsafe_object_references_lock);
    GameEntity id = p_obj->get_instance_id();
    auto elem = unsafe_object_references.find(id);
    ERR_FAIL_COND(elem==unsafe_object_references.end());
    if (--elem->second == 0) {
        unsafe_object_references.erase(elem);
    }
#endif
}

void CSharpLanguage::frame() {

    if (!gdmono || !gdmono->is_runtime_initialized() || gdmono->get_core_api_assembly() == nullptr)
        return;

    const auto &task_scheduler_handle = GDMonoCache::cached_data.task_scheduler_handle;

    if (!task_scheduler_handle)
        return;

    MonoObject *task_scheduler = task_scheduler_handle->get_target();

    if (task_scheduler) {
        MonoException *exc = nullptr;
        CACHED_METHOD_THUNK(GodotTaskScheduler, Activate).invoke(task_scheduler, &exc);

        if (exc) {
            GDMonoUtils::debug_unhandled_exception(exc);
        }
    }
}

struct CSharpScriptDepSort {

    // must support sorting so inheritance works properly (parent must be reloaded first)
    bool operator()(const Ref<CSharpScript> &A, const Ref<CSharpScript> &B) const {
        if (A == B) {
            return false; // shouldn't happen but..
        }
        GDMonoClass *I = B->base;
        while (I) {
            if (I == A->script_class) {
                // A is a base of B
                return true;
            }

            I = I->get_parent_class();
        }

        return false; // not a base
    }
};

void CSharpLanguage::reload_all_scripts() {

#ifdef GD_MONO_HOT_RELOAD
    if (is_assembly_reloading_needed()) {
        GD_MONO_SCOPE_THREAD_ATTACH;
        reload_assemblies(false);
    }
#endif
}

void CSharpLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {

    (void)p_script; // UNUSED

    CRASH_COND(!Engine::get_singleton()->is_editor_hint());

#ifdef TOOLS_ENABLED
    Callable::CallError ce;
    get_godotsharp_editor()->get_node(NodePath("HotReloadAssemblyWatcher"))->call("RestartTimer",nullptr,0,ce);
#endif

#ifdef GD_MONO_HOT_RELOAD
    if (is_assembly_reloading_needed()) {
        GD_MONO_SCOPE_THREAD_ATTACH;
        reload_assemblies(p_soft_reload);
    }
#endif
}

#ifdef GD_MONO_HOT_RELOAD
bool CSharpLanguage::is_assembly_reloading_needed() {

    if (!gdmono->is_runtime_initialized()) {
        return false;
    }

    GDMonoAssembly *proj_assembly = gdmono->get_project_assembly();

    String appname = ProjectSettings::get_singleton()->getT<String>("application/config/name");
    String assembly_name = ProjectSettings::get_singleton()->getT<String>("mono/project/assembly_name");
    if (assembly_name.empty()) {
        String appname_safe = OS::get_singleton()->get_safe_dir_name(appname);
        if (appname_safe.empty()) {
            appname_safe = "UnnamedProject";
        }
        assembly_name = appname_safe;
    }

    assembly_name += ".dll";

    if (proj_assembly) {
        String proj_asm_path = proj_assembly->get_path();

        if (!FileAccess::exists(proj_asm_path)) {
            // Maybe it wasn't loaded from the default path, so check this as well
            proj_asm_path = PathUtils::plus_file(GodotSharpDirs::get_res_temp_assemblies_dir(), assembly_name);
            if (!FileAccess::exists(proj_asm_path)) {
                return false; // No assembly to load
            }
        }

        if (FileAccess::get_modified_time(proj_asm_path) <= proj_assembly->get_modified_time()) {
            return false; // Already up to date
        }
    } else {
        if (!FileAccess::exists(PathUtils::plus_file(GodotSharpDirs::get_res_temp_assemblies_dir(), assembly_name))) {
            return false; // No assembly to load
        }
    }

    return true;
}

void CSharpLanguage::reload_assemblies(bool p_soft_reload) {

    if (!gdmono->is_runtime_initialized()) {
        return;
    }

    // There is no soft reloading with Mono. It's always hard reloading.

    Vector<Ref<CSharpScript> > scripts;

    {
        MutexLock lock(script_instances_mutex);
        for (CSharpScript *elem : script_list) {
            // Cast to CSharpScript to avoid being erased by accident
            scripts.push_back(Ref<CSharpScript>(elem));
        }
    }

    eastl::sort(scripts.begin(), scripts.end(), CSharpScriptDepSort()); // Update in inheritance dependency order

    // Serialize managed callables
    {
        MutexLock lock(ManagedCallable::instances_mutex);

        for (IntrusiveListNode<ManagedCallable> *elem = ManagedCallable::instances.first(); elem; elem = elem->next()) {
            ManagedCallable *managed_callable = elem->self();

            MonoDelegate *delegate = (MonoDelegate *)managed_callable->delegate_handle.get_target();

            Array serialized_data;
            MonoObject *managed_serialized_data = GDMonoMarshal::variant_to_mono_object(serialized_data);

            MonoException *exc = nullptr;
            bool success = (bool)CACHED_METHOD_THUNK(DelegateUtils, TrySerializeDelegate).invoke(delegate, managed_serialized_data, &exc);

            if (exc) {
                GDMonoUtils::debug_print_unhandled_exception(exc);
                continue;
            }

            if (success) {
                ManagedCallable::instances_pending_reload.emplace(managed_callable, serialized_data);
            } else if (OS::get_singleton()->is_stdout_verbose()) {
                OS::get_singleton()->print("Failed to serialize delegate\n");
            }
        }
    }

    List<Ref<CSharpScript>> to_reload;

    // We need to keep reference instances alive during reloading
    List<Ref<RefCounted> > ref_instances;

    for (auto &E : script_bindings) {
        CSharpScriptBinding &script_binding = E.second;
        RefCounted *ref = object_cast<RefCounted>(script_binding.owner);
        if (ref) {
            ref_instances.push_back(Ref<RefCounted>(ref));
        }
    }

    // As scripts are going to be reloaded, must proceed without locking here

    for (Ref<CSharpScript>& script : scripts) {
        // If someone removes a script from a node, deletes the script, builds, adds a script to the
        // same node, then builds again, the script might have no path and also no script_class. In
        // that case, we can't (and don't need to) reload it.
        if (script->get_path().empty() && !script->script_class) {
            continue;
        }
        to_reload.push_back(script);

        if (script->get_path().empty()) {
            script->tied_class_name_for_reload = StringName(script->script_class->get_name_for_lookup());
            script->tied_class_namespace_for_reload = script->script_class->get_namespace();
        }

        // Script::instances are deleted during managed object disposal, which happens on domain finalize.
        // Only placeholders are kept. Therefore we need to keep a copy before that happens.

        for (Object *obj :script->instances) {
            script->pending_reload_instances.insert(obj->get_instance_id());

            RefCounted *ref = object_cast<RefCounted>(obj);
            if (ref) {
                ref_instances.push_back(Ref<RefCounted>(ref));
            }
        }

#ifdef TOOLS_ENABLED
        for (PlaceHolderScriptInstance *F : script->placeholders) {
            Object *obj = F->get_owner();
            script->pending_reload_instances.insert(obj->get_instance_id());

            RefCounted *ref = object_cast<RefCounted>(obj);
            if (ref) {
                ref_instances.push_back(Ref<RefCounted>(ref));
            }
        }
#endif

        // Save state and remove script from instances
        HashMap<GameEntity, CSharpScript::StateBackup> &owners_map = script->pending_reload_state;

        for (Object *obj : script->instances) {

            ERR_CONTINUE(!obj->get_script_instance());

            CSharpInstance *csi = static_cast<CSharpInstance *>(obj->get_script_instance());

            // Call OnBeforeSerialize
            if (csi->script->script_class->implements_interface(CACHED_CLASS(ISerializationListener))) {
                obj->get_script_instance()->call(string_names.on_before_serialize);
            }

            // Save instance info
            CSharpScript::StateBackup state;

            // TODO: Proper state backup (Not only variants, serialize managed state of scripts)
            csi->get_properties_state_for_reloading(state.properties);
            csi->get_event_signals_state_for_reloading(state.event_signals);

            owners_map[obj->get_instance_id()] = state;
        }
    }

    // After the state of all instances is saved, clear scripts and script instances
    for (Ref<CSharpScript>& script : scripts) {

        while (!script->instances.empty()) {
            Object *obj = *script->instances.begin();
            obj->set_script(RefPtr()); // Remove script and existing script instances (placeholder are not removed before domain reload)
        }

        script->_clear();
    }

    // Do domain reload
    if (gdmono->reload_scripts_domain() != OK) {
        // Failed to reload the scripts domain
        // Make sure to add the scripts back to their owners before returning
        for (const Ref<CSharpScript> &scr : to_reload) {

            for (const auto &F : scr->pending_reload_state) {
                Object *obj = object_for_entity(F.first);

                if (!obj) {
                    continue;
                }

                GameEntity obj_id = obj->get_instance_id();

                // Use a placeholder for now to avoid losing the state when saving a scene

                obj->set_script(scr.get_ref_ptr());

                PlaceHolderScriptInstance *placeholder = scr->placeholder_instance_create(obj);
                obj->set_script_instance(placeholder);

#ifdef TOOLS_ENABLED
                // Even though build didn't fail, this tells the placeholder to keep properties and
                // it allows using property_set_fallback for restoring the state without a valid script.
                scr->placeholder_fallback_enabled = true;
#endif

                // Restore Variant properties state, it will be kept by the placeholder until the next script reloading
                for (auto & G : scr->pending_reload_state[obj_id].properties) {
                    placeholder->property_set_fallback(G.first, G.second, nullptr);
                }

                scr->pending_reload_state.erase(obj_id);
            }
        }

        return;
    }

    List<Ref<CSharpScript> > to_reload_state;

    for (Ref<CSharpScript> script : to_reload) {

#ifdef TOOLS_ENABLED
        script->exports_invalidated = true;
#endif
        script->signals_invalidated = true;

        if (!script->get_path().empty()) {

            script->reload(p_soft_reload);
            script->update_exports();

            if (!script->valid) {
                script->pending_reload_instances.clear();
                continue;
            }
        } else {
            const StringName &class_namespace = script->tied_class_namespace_for_reload;
            const StringName &class_name = script->tied_class_name_for_reload;
            GDMonoAssembly *project_assembly = gdmono->get_project_assembly();

            // Search in project and tools assemblies first as those are the most likely to have the class
            GDMonoClass *script_class = (project_assembly ? project_assembly->get_class(class_namespace, class_name) : nullptr);

#ifdef TOOLS_ENABLED
            if (!script_class) {
                GDMonoAssembly *tools_assembly = gdmono->get_tools_assembly();
                script_class = (tools_assembly ? tools_assembly->get_class(class_namespace, class_name) : nullptr);
            }
#endif

            if (!script_class) {
                script_class = gdmono->get_class(class_namespace, class_name);
            }

            if (!script_class) {
                // The class was removed, can't reload
                script->pending_reload_instances.clear();
                continue;
            }

            bool obj_type = CACHED_CLASS(GodotObject)->is_assignable_from(script_class);
            if (!obj_type) {
                // The class no longer inherits Godot.Object, can't reload
                script->pending_reload_instances.clear();
                continue;
            }

            GDMonoClass *native = GDMonoUtils::get_class_native_base(script_class);

            CSharpScript::initialize_for_managed_type(script, script_class, native);
        }

        StringName native_name(NATIVE_GDMONOCLASS_NAME(script->native));

        {
            for (GameEntity obj_id :script->pending_reload_instances) {
                Object *obj = object_for_entity(obj_id);

                if (!obj) {
                    script->pending_reload_state.erase(obj_id);
                    continue;
                }

                if (!ClassDB::is_parent_class(obj->get_class_name(), native_name)) {
                    // No longer inherits the same compatible type, can't reload
                    script->pending_reload_state.erase(obj_id);
                    continue;
                }

                ScriptInstance *si = obj->get_script_instance();

#ifdef TOOLS_ENABLED
                if (si) {
                    // If the script instance is not null, then it must be a placeholder.
                    // Non-placeholder script instances are removed in godot_icall_Object_Disposed.
                    CRASH_COND(!si->is_placeholder());

                    if (script->is_tool() || ScriptServer::is_scripting_enabled()) {
                        // Replace placeholder with a script instance

                        CSharpScript::StateBackup &state_backup = script->pending_reload_state[obj_id];

                        // Backup placeholder script instance state before replacing it with a script instance
                        si->get_property_state(state_backup.properties);

                        ScriptInstance *script_instance = script->instance_create(obj);

                        if (script_instance) {
                            script->placeholders.erase(static_cast<PlaceHolderScriptInstance *>(si));
                            obj->set_script_instance(script_instance);
                        }
                    }

                    continue;
                }
#else
                CRASH_COND(si != nullptr);
#endif
                // Re-create script instance
                obj->set_script(script.get_ref_ptr()); // will create the script instance as well
            }
        }

        to_reload_state.push_back(script);
    }

    for (const Ref<CSharpScript> &script : to_reload_state) {

        for (GameEntity obj_id : script->pending_reload_instances) {
            Object *obj = object_for_entity(obj_id);

            if (!obj) {
                script->pending_reload_state.erase(obj_id);
                continue;
            }

            ERR_CONTINUE(!obj->get_script_instance());

            // TODO: Restore serialized state

            CSharpScript::StateBackup &state_backup = script->pending_reload_state[obj_id];

            for (const Pair<StringName, Variant> &G : state_backup.properties) {
                obj->get_script_instance()->set(G.first, G.second);
            }

            CSharpInstance *csi = CAST_CSHARP_INSTANCE(obj->get_script_instance());
            if (csi) {
                for (const auto &G : state_backup.event_signals) {
                    const StringName &name = G.first;
                    const Array &serialized_data = G.second;

                    auto match = script->event_signals.find(name);

                    if (match==script->event_signals.end()) {
                        // The event or its signal attribute were removed
                        continue;
                    }

                    const CSharpScript::EventSignal &event_signal = match->second;

                    MonoObject *managed_serialized_data = GDMonoMarshal::variant_to_mono_object(serialized_data);
                    MonoDelegate *delegate = nullptr;

                    MonoException *exc = nullptr;
                    bool success = (bool)CACHED_METHOD_THUNK(DelegateUtils, TryDeserializeDelegate).invoke(managed_serialized_data, &delegate, &exc);

                    if (exc) {
                        GDMonoUtils::debug_print_unhandled_exception(exc);
                        continue;
                    }

                    if (success) {
                        ERR_CONTINUE(delegate == nullptr);
                        event_signal.field->set_value(csi->get_mono_object(), (MonoObject *)delegate);
                    } else if (OS::get_singleton()->is_stdout_verbose()) {
                        OS::get_singleton()->print("Failed to deserialize event signal delegate\n");
                    }
                }

                // Call OnAfterDeserialization
                if (csi->script->script_class->implements_interface(CACHED_CLASS(ISerializationListener))) {
                    obj->get_script_instance()->call(string_names.on_after_deserialize);
                }
            }
        }

        script->pending_reload_instances.clear();
    }

    // Deserialize managed callables
    {
        MutexLock lock(ManagedCallable::instances_mutex);

        for (const eastl::pair<ManagedCallable *const , Array> &elem : ManagedCallable::instances_pending_reload) {
            ManagedCallable *managed_callable = elem.first;
            const Array &serialized_data = elem.second;

            MonoObject *managed_serialized_data = GDMonoMarshal::variant_to_mono_object(serialized_data);
            MonoDelegate *delegate = nullptr;

            MonoException *exc = nullptr;
            bool success = (bool)CACHED_METHOD_THUNK(DelegateUtils, TryDeserializeDelegate).invoke(managed_serialized_data, &delegate, &exc);

            if (exc) {
                GDMonoUtils::debug_print_unhandled_exception(exc);
                continue;
            }

            if (success) {
                ERR_CONTINUE(delegate == nullptr);
                managed_callable->set_delegate(delegate);
            } else if (OS::get_singleton()->is_stdout_verbose()) {
                OS::get_singleton()->print("Failed to deserialize delegate\n");
            }
        }

        ManagedCallable::instances_pending_reload.clear();
    }
#ifdef TOOLS_ENABLED
    // FIXME: Hack to refresh editor in order to display new properties and signals. See if there is a better alternative.
    if (Engine::get_singleton()->is_editor_hint()) {
        EditorNode::get_singleton()->get_inspector()->update_tree();
        NodeDock::singleton->update_lists();
    }
#endif
}
#endif

void CSharpLanguage::_load_scripts_metadata() {

    scripts_metadata.clear();

    String scripts_metadata_filename = "scripts_metadata.";

#ifdef TOOLS_ENABLED
    scripts_metadata_filename += Engine::get_singleton()->is_editor_hint() ? "editor" : "editor_player";
#else
#ifdef DEBUG_ENABLED
    scripts_metadata_filename += "debug";
#else
    scripts_metadata_filename += "release";
#endif
#endif

    String scripts_metadata_path = PathUtils::plus_file(GodotSharpDirs::get_res_metadata_dir(),scripts_metadata_filename);

    if (FileAccess::exists(scripts_metadata_path)) {
        String old_json;

        Error ferr = read_all_file_utf8(scripts_metadata_path, old_json);

        ERR_FAIL_COND(ferr != OK);

        Variant old_dict_var;
        String err_str;
        int err_line;
        Error json_err = JSON::parse(old_json, old_dict_var, err_str, err_line);
        if (json_err != OK) {
            ERR_PRINT("Failed to parse metadata file: '" + err_str + "' (" + StringUtils::num_int64(err_line) + ").");
            return;
        }

        scripts_metadata = old_dict_var.as<Dictionary>();
        scripts_metadata_invalidated = false;

        print_verbose("Successfully loaded scripts metadata");
    } else {
        if (!Engine::get_singleton()->is_editor_hint()) {
            ERR_PRINT("Missing scripts metadata file.");
        }
    }
}

void CSharpLanguage::get_recognized_extensions(Vector<String> *p_extensions) const {

    p_extensions->emplace_back("cs");
}

#ifdef TOOLS_ENABLED
Error CSharpLanguage::open_in_external_editor(const Ref<Script> &p_script, int p_line, int p_col) {

    return (Error)(int)get_godotsharp_editor()->call_va("OpenInExternalEditor", (Variant)p_script, p_line, p_col).as<Error>();
}

bool CSharpLanguage::overrides_external_editor() {

    return get_godotsharp_editor()->call_va("OverridesExternalEditor").as<bool>();
}
#endif

void CSharpLanguage::thread_enter() {

#if 0
    if (gdmono->is_runtime_initialized()) {
        GDMonoUtils::attach_current_thread();
    }
#endif
}

void CSharpLanguage::thread_exit() {

#if 0
    if (gdmono->is_runtime_initialized()) {
        GDMonoUtils::detach_current_thread();
    }
#endif
}

bool CSharpLanguage::debug_break_parse(StringView p_file, int p_line, const String &p_error) {

    // Not a parser error in our case, but it's still used for other type of errors
    if (ScriptDebugger::get_singleton() && Thread::get_caller_id() == Thread::get_main_id()) {
        _debug_parse_err_line = p_line;
        _debug_parse_err_file = p_file;
        _debug_error = p_error;
        ScriptDebugger::get_singleton()->debug(this, false, true);
        return true;
    } else {
        return false;
    }
}

bool CSharpLanguage::debug_break(const String &p_error, bool p_allow_continue) {

    if (!ScriptDebugger::get_singleton() || Thread::get_caller_id() != Thread::get_main_id()) {
        return false;
    }
    _debug_parse_err_line = -1;
    _debug_parse_err_file = "";
    _debug_error = p_error;
    ScriptDebugger::get_singleton()->debug(this, p_allow_continue);
    return true;
}

void CSharpLanguage::_on_scripts_domain_unloaded() {
    for (auto & E : script_bindings) {
        CSharpScriptBinding &script_binding = E.second;
        script_binding.gchandle.release();
        script_binding.inited = false;
    }

#ifdef GD_MONO_HOT_RELOAD
    {
        MutexLock lock(ManagedCallable::instances_mutex);

        for (IntrusiveListNode<ManagedCallable> *elem = ManagedCallable::instances.first(); elem; elem = elem->next()) {
            ManagedCallable *managed_callable = elem->self();
            managed_callable->delegate_handle.release();
            managed_callable->delegate_invoke = nullptr;
        }
    }
#endif
    scripts_metadata_invalidated = true;
}

#ifdef TOOLS_ENABLED
void CSharpLanguage::_editor_init_callback() {

    register_editor_internal_calls();

    // Initialize GodotSharpEditor
    GDMonoAssembly *toolsassembly= GDMono::get_singleton()->get_tools_assembly();
    CRASH_COND(toolsassembly == nullptr);

    GDMonoClass *editor_klass = toolsassembly->get_class("GodotTools", "GodotSharpEditor");
    CRASH_COND(editor_klass == nullptr);
    MonoObject *mono_object = mono_object_new(mono_domain_get(), editor_klass->get_mono_ptr());
    CRASH_COND(mono_object == nullptr);

    MonoException *exc = nullptr;
    GDMonoUtils::runtime_object_init(mono_object, editor_klass, &exc);
    UNHANDLED_EXCEPTION(exc);


    EditorPlugin *godotsharp_editor = GDMonoMarshal::mono_object_to_variant(mono_object).asT<EditorPlugin>();
    CRASH_COND(godotsharp_editor == nullptr);

    // Enable it as a plugin
    EditorNode::add_editor_plugin(godotsharp_editor);
    ED_SHORTCUT("mono/build_solution", TTR("Build Solution"), KEY_MASK_ALT | KEY_B);
    godotsharp_editor->enable_plugin();

    get_singleton()->godotsharp_editor = godotsharp_editor;
}
#endif

void CSharpLanguage::set_language_index(int p_idx) {

    ERR_FAIL_COND(lang_idx != -1);
    lang_idx = p_idx;
}

void CSharpLanguage::release_script_gchandle(MonoGCHandleData &p_gchandle) {

    if (!p_gchandle.is_released()) { // Do not lock unnecessarily
        MutexLock lock(get_singleton()->script_gchandle_release_mutex);
        p_gchandle.release();
    }
}

void CSharpLanguage::release_script_gchandle(MonoObject *p_expected_obj, MonoGCHandleData &p_gchandle) {
    uint32_t pinned_gchandle = GDMonoUtils::new_strong_gchandle_pinned(p_expected_obj); // We might lock after this, so pin it

    if (!p_gchandle.is_released()) { // Do not lock unnecessarily
        MutexLock lock(get_singleton()->script_gchandle_release_mutex);

        MonoObject *target = p_gchandle.get_target();

        // We release the gchandle if it points to the MonoObject* we expect (otherwise it was
        // already released and could have been replaced) or if we can't get its target MonoObject*
        // (which doesn't necessarily mean it was released, and we want it released in order to
        // avoid locking other threads unnecessarily).
        if (target == p_expected_obj || target == nullptr) {
            p_gchandle.release();
        }
    }

    GDMonoUtils::free_gchandle(pinned_gchandle);
}

CSharpLanguage::CSharpLanguage() {

    ERR_FAIL_COND_MSG(singleton, "C# singleton already exist.");
    singleton = this;

}

CSharpLanguage::~CSharpLanguage() {

    finalize();
    singleton = nullptr;
}

bool CSharpLanguage::setup_csharp_script_binding(CSharpScriptBinding &r_script_binding, Object *p_object) {

#ifdef DEBUG_ENABLED
    // I don't trust you
    if (p_object->get_script_instance()) {
        CSharpInstance *csharp_instance = CAST_CSHARP_INSTANCE(p_object->get_script_instance());
        CRASH_COND(csharp_instance != nullptr && !csharp_instance->is_destructing_script_instance());
    }
#endif

    StringName type_name = p_object->get_class_name();

    // ¯\_(ツ)_/¯
    auto iter = ClassDB::classes.find(type_name);
    auto classinfo = iter!=ClassDB::classes.end() ? &iter->second : nullptr;
    while (classinfo && !classinfo->exposed) {
        classinfo = classinfo->inherits_ptr;
    }
    ERR_FAIL_NULL_V(classinfo, false);
    type_name = classinfo->name;

    GDMonoClass *type_class = GDMonoUtils::type_get_proxy_class(type_name);

    ERR_FAIL_NULL_V(type_class, false);

    MonoObject *mono_object = GDMonoUtils::create_managed_for_godot_object(type_class, type_name, p_object);

    ERR_FAIL_NULL_V(mono_object, false);

    r_script_binding.inited = true;
    r_script_binding.type_name = type_name;
    r_script_binding.wrapper_class = type_class; // cache
    r_script_binding.gchandle = MonoGCHandleData::new_strong_handle(mono_object);
    r_script_binding.owner = p_object;

    // Tie managed to unmanaged
    RefCounted *ref = object_cast<RefCounted>(p_object);

    if (ref) {
        // Unsafe refcount increment. The managed instance also counts as a reference.
        // This way if the unmanaged world has no references to our owner
        // but the managed instance is alive, the refcount will be 1 instead of 0.
        // See: godot_icall_Reference_Dtor(MonoObject *p_obj, Object *p_ptr)

        ref->reference();
        CSharpLanguage::get_singleton()->post_unsafe_reference(ref);
    }

    return true;
}

void *CSharpLanguage::alloc_instance_binding_data(Object *p_object) {
    MutexLock lock(language_bind_mutex);

    auto itermatch = script_bindings.find(p_object);
    if (itermatch!=script_bindings.end()) {
        return itermatch.mpNode;
    }

    CSharpScriptBinding script_binding;

    if (!setup_csharp_script_binding(script_binding, p_object)) {
        return nullptr;
    }

    return insert_script_binding(p_object, script_binding).mpNode;
}

Map<Object *, CSharpScriptBinding>::iterator CSharpLanguage::insert_script_binding(Object *p_object, const CSharpScriptBinding &p_script_binding) {
    return script_bindings.emplace(p_object, p_script_binding).first;
}
void CSharpLanguage::free_instance_binding_data(void *p_data) {

    if (GDMono::get_singleton() == nullptr) {
#ifdef DEBUG_ENABLED
        CRASH_COND(!script_bindings.empty());
#endif
        // Mono runtime finalized, all the gchandle bindings were already released
        return;
    }

    if (finalizing) {
        return; // inside CSharpLanguage::finish(), all the gchandle bindings are released there
    }

    GD_MONO_ASSERT_THREAD_ATTACHED;

    {
        MutexLock lock(language_bind_mutex);
        Map<Object *, CSharpScriptBinding>::iterator iter;
        iter.mpNode = (Map<Object *, CSharpScriptBinding>::iterator::node_type *)p_data;
        CSharpScriptBinding &script_binding = iter->second;

        if (script_binding.inited) {
            // Set the native instance field to IntPtr.Zero, if not yet garbage collected.
            // This is done to avoid trying to dispose the native instance from Dispose(bool).
            MonoObject *mono_object = script_binding.gchandle.get_target();
            if (mono_object) {
                CACHED_FIELD(GodotObject, ptr)->set_value_raw(mono_object, nullptr);
            }
            script_binding.gchandle.release();
        }

        script_bindings.erase(iter);
    }
}

void CSharpLanguage::refcount_incremented_instance_binding(Object *p_object) {

    RefCounted *ref_owner = object_cast<RefCounted>(p_object);

#ifdef DEBUG_ENABLED
    CRASH_COND(!ref_owner);
    CRASH_COND(!p_object->has_script_instance_binding(get_language_index()));
#endif

    void *data = p_object->get_script_instance_binding(get_language_index());
    CRASH_COND(!data);

    Map<Object *, CSharpScriptBinding>::iterator iter;
    iter.mpNode = (Map<Object *, CSharpScriptBinding>::iterator::node_type *)data;
    CSharpScriptBinding &script_binding = iter->second;
    MonoGCHandleData &gchandle = script_binding.gchandle;

    if (!script_binding.inited) {
        return;
    }

    if (ref_owner->reference_get_count() > 1 && gchandle.is_weak()) { // The managed side also holds a reference, hence 1 instead of 0
        GD_MONO_SCOPE_THREAD_ATTACH;
        // The reference count was increased after the managed side was the only one referencing our owner.
        // This means the owner is being referenced again by the unmanaged side,
        // so the owner must hold the managed side alive again to avoid it from being GCed.

        MonoObject *target = gchandle.get_target();
        if (!target) {
            return; // Called after the managed side was collected, so nothing to do here
        }

        // Release the current weak handle and replace it with a strong handle.
        MonoGCHandleData strong_gchandle = MonoGCHandleData::new_strong_handle(target);
        gchandle.release();
        gchandle = strong_gchandle;
    }
}

bool CSharpLanguage::refcount_decremented_instance_binding(Object *p_object) {

    RefCounted *ref_owner = object_cast<RefCounted>(p_object);

#ifdef DEBUG_ENABLED
    CRASH_COND(!ref_owner);
    CRASH_COND(!p_object->has_script_instance_binding(get_language_index()));
#endif

    void *data = p_object->get_script_instance_binding(get_language_index());
    CRASH_COND(!data);

    Map<Object *, CSharpScriptBinding>::iterator iter;
    iter.mpNode = (Map<Object *, CSharpScriptBinding>::iterator::node_type *)data;
    CSharpScriptBinding &script_binding = iter->second;
    MonoGCHandleData &gchandle = script_binding.gchandle;

    int refcount = ref_owner->reference_get_count();

    if (!script_binding.inited) {
        return refcount == 0;
    }

    if (refcount == 1 && !gchandle.is_released() && !gchandle.is_weak()) { // The managed side also holds a reference, hence 1 instead of 0
        GD_MONO_SCOPE_THREAD_ATTACH;
        // If owner owner is no longer referenced by the unmanaged side,
        // the managed instance takes responsibility of deleting the owner when GCed.

        MonoObject *target = gchandle.get_target();
        if (!target) {
            //TODO: this will always return false, since refcount was 1 on entry to enclosing if-scope
            return refcount == 0; // Called after the managed side was collected, so nothing to do here
        }

        // Release the current strong handle and replace it with a weak handle.
        MonoGCHandleData weak_gchandle = MonoGCHandleData::new_weak_handle(target);
        gchandle.release();
        gchandle = weak_gchandle;

        return false;
    }

    return refcount == 0;
}

CSharpInstance *CSharpInstance::create_for_managed_type(Object *p_owner, CSharpScript *p_script, const MonoGCHandleData &p_gchandle) {
    CSharpInstance *instance = memnew(CSharpInstance(Ref<CSharpScript>(p_script)));

    RefCounted *ref = object_cast<RefCounted>(p_owner);

    instance->base_ref = ref != nullptr;
    instance->owner = p_owner;
    instance->gchandle = p_gchandle;

    if (instance->base_ref) {
        instance->_reference_owner_unsafe();
    }

    p_script->instances.insert(p_owner);

    return instance;
}

MonoObject *CSharpInstance::get_mono_object() const {

    ERR_FAIL_COND_V(gchandle.is_released(), nullptr);
    return gchandle.get_target();
}

Object *CSharpInstance::get_owner() {
    return owner;
}

bool CSharpInstance::set(const StringName &p_name, const Variant &p_value) {

    ERR_FAIL_COND_V(!script, false);

    GD_MONO_SCOPE_THREAD_ATTACH;

    MonoObject *mono_object = get_mono_object();
    ERR_FAIL_NULL_V(mono_object, false);

    GDMonoClass *top = script->script_class;

    while (top && top != script->native) {
        GDMonoField *field = top->get_field(p_name);

        if (field) {
            field->set_value_from_variant(mono_object, p_value);
            return true;
        }

        GDMonoProperty *property = top->get_property(p_name);

        if (property) {
            property->set_value_from_variant(mono_object, p_value);
            return true;
        }

        top = top->get_parent_class();
    }

    // Call _set

    top = script->script_class;

    while (top && top != script->native) {
        GDMonoMethod *method = top->get_method(CACHED_STRING_NAME(_set), 2);

        if (method) {
            Variant name = p_name;
            const Variant *args[2] = { &name, &p_value };

            MonoObject *ret = method->invoke(mono_object, args);

            if (ret && GDMonoMarshal::unbox<MonoBoolean>(ret)) {
                return true;
            }

            break;
        }

        top = top->get_parent_class();
    }

    return false;
}

bool CSharpInstance::get(const StringName &p_name, Variant &r_ret) const {

    ERR_FAIL_COND_V(!script, false);

    GD_MONO_SCOPE_THREAD_ATTACH;

    MonoObject *mono_object = get_mono_object();
    ERR_FAIL_NULL_V(mono_object, false);

    GDMonoClass *top = script->script_class;

    while (top && top != script->native) {
        GDMonoField *field = top->get_field(p_name);

        if (field) {
            MonoObject *value = field->get_value(mono_object);
            r_ret = GDMonoMarshal::mono_object_to_variant(value);
            return true;
        }

        GDMonoProperty *property = top->get_property(p_name);

        if (property) {
            MonoException *exc = nullptr;
            MonoObject *value = property->get_value(mono_object, &exc);
            if (exc) {
                r_ret = Variant();
                GDMonoUtils::set_pending_exception(exc);
            } else {
                r_ret = GDMonoMarshal::mono_object_to_variant(value);
            }
            return true;
        }

        top = top->get_parent_class();
    }

    // Call _get

    top = script->script_class;

    while (top && top != script->native) {
        GDMonoMethod *method = top->get_method(CACHED_STRING_NAME(_get), 1);

        if (method) {
            Variant name = p_name;
            const Variant *args[1] = { &name };

            MonoObject *ret = method->invoke(mono_object, args);

            if (ret) {
                r_ret = GDMonoMarshal::mono_object_to_variant(ret);
                return true;
            }

            break;
        }

        top = top->get_parent_class();
    }

    return false;
}

void CSharpInstance::get_properties_state_for_reloading(Vector<Pair<StringName, Variant>> &r_state) {

    Vector<PropertyInfo> pinfo;
    get_property_list(&pinfo);

    for (const PropertyInfo &E : pinfo) {
        Pair<StringName, Variant> state_pair;
        state_pair.first = E.name;

        ManagedType managedType;

        GDMonoField *field = nullptr;
        GDMonoClass *top = script->script_class;
        while (top && top != script->native) {
            field = top->get_field(state_pair.first);
            if (field) {
                break;
            }

            top = top->get_parent_class();
        }
        if (!field) {
            continue; // Properties ignored. We get the property baking fields instead.
        }

        managedType = field->get_type();

        if (GDMonoMarshal::managed_to_variant_type(managedType) != VariantType::NIL) { // If we can marshal it
            if (get(state_pair.first, state_pair.second)) {
                r_state.push_back(state_pair);
            }
        }
    }
}
void CSharpInstance::get_event_signals_state_for_reloading(Vector<Pair<StringName, Array>> &r_state) {
    MonoObject *owner_managed = get_mono_object();
    ERR_FAIL_NULL(owner_managed);

    for (const auto &E : script->event_signals) {
        const CSharpScript::EventSignal &event_signal = E.second;

        MonoDelegate *delegate_field_value = (MonoDelegate *)event_signal.field->get_value(owner_managed);
        if (!delegate_field_value) {
            continue; // Empty
        }

        Array serialized_data;
        MonoObject *managed_serialized_data = GDMonoMarshal::variant_to_mono_object(serialized_data);

        MonoException *exc = nullptr;
        bool success = (bool)CACHED_METHOD_THUNK(DelegateUtils, TrySerializeDelegate).invoke(delegate_field_value, managed_serialized_data, &exc);

        if (exc) {
            GDMonoUtils::debug_print_unhandled_exception(exc);
            continue;
        }

        if (success) {
            r_state.push_back(Pair<StringName, Array>(event_signal.field->get_name(), serialized_data));
        } else if (OS::get_singleton()->is_stdout_verbose()) {
            OS::get_singleton()->print("Failed to serialize event signal delegate\n");
        }
    }
}
void CSharpInstance::get_property_list(Vector<PropertyInfo> *p_properties) const {

    for (const auto &E : script->member_info) {
        p_properties->push_back(E.second);
    }

    // Call _get_property_list

    ERR_FAIL_COND(!script);

    GD_MONO_SCOPE_THREAD_ATTACH;

    MonoObject *mono_object = get_mono_object();
    ERR_FAIL_NULL(mono_object);

    GDMonoClass *top = script->script_class;

    while (top && top != script->native) {
        GDMonoMethod *method = top->get_method(CACHED_STRING_NAME(_get_property_list), 0);

        if (method) {
            MonoObject *ret = method->invoke(mono_object);

            if (ret) {
                Array array = GDMonoMarshal::mono_object_to_variant(ret).as<Array>();
                for (int i = 0, size = array.size(); i < size; i++) {
                    p_properties->push_back(PropertyInfo::from_dict(array.get(i).as<Dictionary>()));
                }
            }

            break;
        }

        top = top->get_parent_class();
    }
}

VariantType CSharpInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {

    if (script->member_info.contains(p_name)) {
        if (r_is_valid) {
            *r_is_valid = true;
        }
        return script->member_info[p_name].type;
    }

    if (r_is_valid) {
        *r_is_valid = false;
    }

    return VariantType::NIL;
}

void CSharpInstance::get_method_list(Vector<MethodInfo> *p_list) const {
    if (!script->is_valid() || !script->script_class) {
        return;
    }

    GD_MONO_SCOPE_THREAD_ATTACH;

    // TODO: We're filtering out constructors but there may be other methods unsuitable for explicit calls.
    GDMonoClass *top = script->script_class;

    while (top && top != script->native) {
        const Vector<GDMonoMethod *> &methods = top->get_all_methods();
        for (int i = 0; i < methods.size(); ++i) {
            MethodInfo minfo = methods[i]->get_method_info();
            if (minfo.name != CACHED_STRING_NAME(dotctor)) {
                p_list->push_back(minfo);
            }
        }

        top = top->get_parent_class();
    }
}

bool CSharpInstance::has_method(const StringName &p_method) const {

    if (!script) {
        return false;
    }

    GD_MONO_SCOPE_THREAD_ATTACH;

    GDMonoClass *top = script->script_class;

    while (top && top != script->native) {
        if (top->has_fetched_method_unknown_params(p_method)) {
            return true;
        }

        top = top->get_parent_class();
    }

    return false;
}

Variant CSharpInstance::call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
    ERR_FAIL_COND_V(!script, Variant());

    GD_MONO_SCOPE_THREAD_ATTACH;

    MonoObject *mono_object = get_mono_object();

    if (!mono_object) {
        r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
        ERR_FAIL_V(Variant());
    }

    GDMonoClass *top = script->script_class;

    while (top && top != script->native) {
        GDMonoMethod *method = top->get_method(p_method, p_argcount);

        if (method) {
            MonoObject *return_value = method->invoke(mono_object, p_args);

            r_error.error = Callable::CallError::CALL_OK;

            if (return_value) {
                return GDMonoMarshal::mono_object_to_variant(return_value);
            } else {
                return Variant();
            }
        }

        top = top->get_parent_class();
    }

    r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;

    return Variant();
}

bool CSharpInstance::_reference_owner_unsafe() {

#ifdef DEBUG_ENABLED
    CRASH_COND(!base_ref);
    CRASH_COND(owner == nullptr);
    CRASH_COND(unsafe_referenced); // already referenced
#endif

    // Unsafe refcount increment. The managed instance also counts as a reference.
    // This way if the unmanaged world has no references to our owner
    // but the managed instance is alive, the refcount will be 1 instead of 0.
    // See: _unreference_owner_unsafe()

    // May not me referenced yet, so we must use init_ref() instead of reference()
    if (static_cast<RefCounted *>(owner)->init_ref()) {
        CSharpLanguage::get_singleton()->post_unsafe_reference(owner);
        unsafe_referenced = true;
    }

    return unsafe_referenced;
}

bool CSharpInstance::_unreference_owner_unsafe() {

#ifdef DEBUG_ENABLED
    CRASH_COND(!base_ref);
    CRASH_COND(owner == nullptr);
#endif

    if (!unsafe_referenced) {
        return false; // Already unreferenced
    }

    unsafe_referenced = false;

    // Called from CSharpInstance::mono_object_disposed() or ~CSharpInstance()

    // Unsafe refcount decrement. The managed instance also counts as a reference.
    // See: _reference_owner_unsafe()

    // Destroying the owner here means self destructing, so we defer the owner destruction to the caller.
    CSharpLanguage::get_singleton()->pre_unsafe_unreference(owner);
    return static_cast<RefCounted *>(owner)->unreference();
}

MonoObject *CSharpInstance::_internal_new_managed() {

    // Search the constructor first, to fail with an error if it's not found before allocating anything else.
    GDMonoMethod *ctor = script->script_class->get_method(CACHED_STRING_NAME(dotctor), 0);
    ERR_FAIL_NULL_V_MSG(ctor, nullptr,
            "Cannot create script instance because the class does not define a parameterless constructor: '" + script->get_path() + "'.");

    CSharpLanguage::get_singleton()->release_script_gchandle(gchandle);

    ERR_FAIL_NULL_V(owner, nullptr);
    ERR_FAIL_COND_V(!script, nullptr);

    MonoObject *mono_object = mono_object_new(mono_domain_get(), script->script_class->get_mono_ptr());

    if (!mono_object) {
        // Important to clear this before destroying the script instance here
        script = Ref<CSharpScript>();

        bool die = _unreference_owner_unsafe();
        // Not ok for the owner to die here. If there is a situation where this can happen, it will be considered a bug.
        CRASH_COND(die);

        owner = nullptr;

        ERR_FAIL_V_MSG(nullptr, "Failed to allocate memory for the object.");
    }

    // Tie managed to unmanaged
    gchandle = MonoGCHandleData::new_strong_handle(mono_object);

    if (base_ref) {
        _reference_owner_unsafe(); // Here, after assigning the gchandle (for the refcount_incremented callback)
    }

    CACHED_FIELD(GodotObject, ptr)->set_value_raw(mono_object, owner);

    // Construct
    ctor->invoke_raw(mono_object, nullptr);

    return mono_object;
}

void CSharpInstance::mono_object_disposed(MonoObject *p_obj) {
    // Must make sure event signals are not left dangling
    disconnect_event_signals();

#ifdef DEBUG_ENABLED
    CRASH_COND(base_ref);
    CRASH_COND(gchandle.is_released());
#endif
    CSharpLanguage::get_singleton()->release_script_gchandle(p_obj, gchandle);
}

void CSharpInstance::mono_object_disposed_baseref(MonoObject *p_obj, bool p_is_finalizer, bool &r_delete_owner, bool &r_remove_script_instance) {

#ifdef DEBUG_ENABLED
    CRASH_COND(!base_ref);
    CRASH_COND(gchandle.is_released());
#endif

    // Must make sure event signals are not left dangling
    disconnect_event_signals();
    r_remove_script_instance = false;

    if (_unreference_owner_unsafe()) {
        // Safe to self destruct here with memdelete(owner), but it's deferred to the caller to prevent future mistakes.
        r_delete_owner = true;
    } else {
        r_delete_owner = false;
        CSharpLanguage::get_singleton()->release_script_gchandle(p_obj, gchandle);

        if (!p_is_finalizer) {
            // If the native instance is still alive and Dispose() was called
            // (instead of the finalizer), then we remove the script instance.
            r_remove_script_instance = true;
        } else if (!GDMono::get_singleton()->is_finalizing_scripts_domain()) {
            // If the native instance is still alive and this is called from the finalizer,
            // then it was referenced from another thread before the finalizer could
            // unreference and delete it, so we want to keep it.
            // GC.ReRegisterForFinalize(this) is not safe because the objects referenced by 'this'
            // could have already been collected. Instead we will create a new managed instance here.
            MonoObject *new_managed = _internal_new_managed();
            if (!new_managed) {
                r_remove_script_instance = true;
            }
        }
    }
}

void CSharpInstance::connect_event_signals() {
    for (const auto &E : script->event_signals) {
        const CSharpScript::EventSignal &event_signal = E.second;

        StringName signal_name = event_signal.field->get_name();

        // TODO: Use pooling for ManagedCallable instances.
        auto event_signal_callable = memnew(EventSignalCallable(owner, &event_signal));

        owner->connect(signal_name, Callable(event_signal_callable));
    }
}

void CSharpInstance::disconnect_event_signals() {
    for (const auto &E : script->event_signals) {
        const CSharpScript::EventSignal &event_signal = E.second;

        StringName signal_name = event_signal.field->get_name();

        // TODO: It would be great if we could store this EventSignalCallable on the stack.
        // The problem is that Callable memdeletes it when it's destructed...
        auto event_signal_callable = memnew(EventSignalCallable(owner, &event_signal));

        owner->disconnect(signal_name, Callable(event_signal_callable));
    }
}
void CSharpInstance::refcount_incremented() {

#ifdef DEBUG_ENABLED
    CRASH_COND(!base_ref);
    CRASH_COND(owner == nullptr);
#endif

    RefCounted *ref_owner = object_cast<RefCounted>(owner);

    if (ref_owner->reference_get_count() > 1 && gchandle.is_weak()) { // The managed side also holds a reference, hence 1 instead of 0
        GD_MONO_SCOPE_THREAD_ATTACH;
        // The reference count was increased after the managed side was the only one referencing our owner.
        // This means the owner is being referenced again by the unmanaged side,
        // so the owner must hold the managed side alive again to avoid it from being GCed.

        // Release the current weak handle and replace it with a strong handle.
        MonoGCHandleData strong_gchandle = MonoGCHandleData::new_strong_handle(gchandle.get_target());
        gchandle.release();
        gchandle = strong_gchandle;
    }
}

bool CSharpInstance::refcount_decremented() {

#ifdef DEBUG_ENABLED
    CRASH_COND(!base_ref);
    CRASH_COND(owner == nullptr);
#endif

    RefCounted *ref_owner = object_cast<RefCounted>(owner);

    int refcount = ref_owner->reference_get_count();

    if (refcount == 1 && !gchandle.is_weak()) { // The managed side also holds a reference, hence 1 instead of 0
        GD_MONO_SCOPE_THREAD_ATTACH;
        // If owner owner is no longer referenced by the unmanaged side,
        // the managed instance takes responsibility of deleting the owner when GCed.

        // Release the current strong handle and replace it with a weak handle.
        MonoGCHandleData weak_gchandle = MonoGCHandleData::new_weak_handle(gchandle.get_target());
        gchandle.release();
        gchandle = weak_gchandle;

        return false;
    }

    ref_dying = (refcount == 0);

    return ref_dying;
}

Vector<ScriptNetData> CSharpInstance::get_rpc_methods() const {
    return script->get_rpc_methods();
}

uint16_t CSharpInstance::get_rpc_method_id(const StringName &p_method) const {
    return script->get_rpc_method_id(p_method);
}

StringName CSharpInstance::get_rpc_method(const uint16_t p_rpc_method_id) const {
    return script->get_rpc_method(p_rpc_method_id);
}

MultiplayerAPI_RPCMode CSharpInstance::get_rpc_mode_by_id(const uint16_t p_rpc_method_id) const {
    return script->get_rpc_mode_by_id(p_rpc_method_id);
}

MultiplayerAPI_RPCMode CSharpInstance::get_rpc_mode(const StringName &p_method) const {
    return script->get_rpc_mode(p_method);
}

Vector<ScriptNetData> CSharpInstance::get_rset_properties() const {
    return script->get_rset_properties();
}

uint16_t CSharpInstance::get_rset_property_id(const StringName &p_variable) const {
    return script->get_rset_property_id(p_variable);
    }

StringName CSharpInstance::get_rset_property(const uint16_t p_rset_member_id) const {
    return script->get_rset_property(p_rset_member_id);
}

MultiplayerAPI_RPCMode CSharpInstance::get_rset_mode_by_id(const uint16_t p_rset_member_id) const {
    return script->get_rset_mode_by_id(p_rset_member_id);
    }

MultiplayerAPI_RPCMode CSharpInstance::get_rset_mode(const StringName &p_variable) const {
    return script->get_rset_mode(p_variable);
}

void CSharpInstance::notification(int p_notification) {
    GD_MONO_SCOPE_THREAD_ATTACH;

    if (p_notification == Object::NOTIFICATION_PREDELETE) {
        // When NOTIFICATION_PREDELETE is sent, we also take the chance to call Dispose().
        // It's safe to call Dispose() multiple times and NOTIFICATION_PREDELETE is guaranteed
        // to be sent at least once, which happens right before the call to the destructor.

        predelete_notified = true;

        if (base_ref) {
            // It's not safe to proceed if the owner derives RefCounted and the refcount reached 0.
            // At this point, Dispose() was already called (manually or from the finalizer) so
            // that's not a problem. The refcount wouldn't have reached 0 otherwise, since the
            // managed side references it and Dispose() needs to be called to release it.
            // However, this means C# RefCounted scripts can't receive NOTIFICATION_PREDELETE, but
            // this is likely the case with GDScript as well: https://github.com/godotengine/godot/issues/6784
            return;
        }

        _call_notification(p_notification);

        MonoObject *mono_object = get_mono_object();
        ERR_FAIL_NULL(mono_object);

        MonoException *exc = nullptr;
        GDMonoUtils::dispose(mono_object, &exc);

        if (exc) {
            GDMonoUtils::set_pending_exception(exc);
        }

        return;
    }

    _call_notification(p_notification);
}

void CSharpInstance::_call_notification(int p_notification) {
    GD_MONO_ASSERT_THREAD_ATTACHED;

    MonoObject *mono_object = get_mono_object();
    ERR_FAIL_NULL(mono_object);

    // Custom version of _call_multilevel, optimized for _notification

    int32_t arg = p_notification;
    void *args[1] = { &arg };
    StringName method_name = CACHED_STRING_NAME(_notification);

    GDMonoClass *top = script->script_class;

    while (top && top != script->native) {
        GDMonoMethod *method = top->get_method(method_name, 1);

        if (method) {
            method->invoke_raw(mono_object, args);
            return;
        }

        top = top->get_parent_class();
    }
}

String CSharpInstance::to_string(bool *r_valid) {
    GD_MONO_SCOPE_THREAD_ATTACH;

    MonoObject *mono_object = get_mono_object();

    if (mono_object == nullptr) {
        if (r_valid) {
            *r_valid = false;
        }
        return String();
    }

    MonoException *exc = nullptr;
    MonoString *result = GDMonoUtils::object_to_string(mono_object, &exc);

    if (exc) {
        GDMonoUtils::set_pending_exception(exc);
        if (r_valid) {
            *r_valid = false;
        }
        return String();
    }

    if (result == nullptr) {
        if (r_valid) {
            *r_valid = false;
        }
        return String();
    }

    return GDMonoMarshal::mono_string_to_godot(result);
}

Ref<Script> CSharpInstance::get_script() const {

    return script;
}

ScriptLanguage *CSharpInstance::get_language() {

    return CSharpLanguage::get_singleton();
}

CSharpInstance::CSharpInstance(const Ref<CSharpScript> &p_script) :
        script(p_script) {
}

CSharpInstance::~CSharpInstance() {
    GD_MONO_SCOPE_THREAD_ATTACH;

    destructing_script_instance = true;
    // Must make sure event signals are not left dangling
    disconnect_event_signals();

    if (!gchandle.is_released()) {
        if (!predelete_notified && !ref_dying) {
            // This destructor is not called from the owners destructor.
            // This could be being called from the owner's set_script_instance method,
            // meaning this script is being replaced with another one. If this is the case,
            // we must call Dispose here, because Dispose calls owner->set_script_instance(nullptr)
            // and that would mess up with the new script instance if called later.

            MonoObject *mono_object = gchandle.get_target();

            if (mono_object) {
                MonoException *exc = nullptr;
                GDMonoUtils::dispose(mono_object, &exc);

                if (exc) {
                    GDMonoUtils::set_pending_exception(exc);
                }
            }
        }

        gchandle.release(); // Make sure the gchandle is released
    }

    // If not being called from the owner's destructor, and we still hold a reference to the owner
    if (base_ref && !ref_dying && owner && unsafe_referenced) {
        // The owner's script or script instance is being replaced (or removed)

        // Transfer ownership to an "instance binding"
        RefCounted *ref_owner = static_cast<RefCounted *>(owner);

        // We will unreference the owner before referencing it again, so we need to keep it alive
        Ref<RefCounted> scope_keep_owner_alive(ref_owner);
        (void)scope_keep_owner_alive;

        // Unreference the owner here, before the new "instance binding" references it.
        // Otherwise, the unsafe reference debug checks will incorrectly detect a bug.
        bool die = _unreference_owner_unsafe();
        CRASH_COND(die); // `owner_keep_alive` holds a reference, so it can't die


        void *data = owner->get_script_instance_binding(CSharpLanguage::get_singleton()->get_language_index());
        CRASH_COND(data == nullptr);

        Map<Object *, CSharpScriptBinding>::iterator iter;
        iter.mpNode = (Map<Object *, CSharpScriptBinding>::iterator::node_type *)data;
        CSharpScriptBinding &script_binding = iter->second;

        if (!script_binding.inited) {
            MutexLock lock(CSharpLanguage::get_singleton()->get_language_bind_mutex());

            if (!script_binding.inited) { // Other thread may have set it up
                // Already had a binding that needs to be setup
                CSharpLanguage::get_singleton()->setup_csharp_script_binding(script_binding, owner);
                CRASH_COND(!script_binding.inited);
            }
        }

#ifdef DEBUG_ENABLED
        // The "instance binding" holds a reference so the refcount should be at least 2 before `scope_keep_owner_alive` goes out of scope
        CRASH_COND(ref_owner->reference_get_count() <= 1);
#endif
    }

    if (script && owner) {
        MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);

#ifdef DEBUG_ENABLED
        // CSharpInstance must not be created unless it's going to be added to the list for sure
        auto match = script->instances.find(owner);
        CRASH_COND(match==script->instances.end());
        script->instances.erase(match);
#else
        script->instances.erase(owner);
#endif
    }
}

#ifdef TOOLS_ENABLED
void CSharpScript::_placeholder_erased(PlaceHolderScriptInstance *p_placeholder) {

    placeholders.erase(p_placeholder);
}
#endif

#ifdef TOOLS_ENABLED
void CSharpScript::_update_exports_values(HashMap<StringName, Variant> &values, Vector<PropertyInfo> &propnames) {

    if (base_cache) {
        base_cache->_update_exports_values(values, propnames);
    }

    for (const auto &E : exported_members_defval_cache) {
        values[E.first] = E.second;
    }

    for (const PropertyInfo &E : exported_members_cache) {
        propnames.push_back(E);
    }
}

void CSharpScript::_update_member_info_no_exports() {

    if (!exports_invalidated)
        return;

    GD_MONO_ASSERT_THREAD_ATTACHED;
    exports_invalidated = false;

    member_info.clear();

    GDMonoClass *top = script_class;

    while (top && top != native) {
        PropertyInfo prop_info;
        bool exported;

        const Vector<GDMonoField *> &fields = top->get_all_fields();

        for (int i = fields.size() - 1; i >= 0; i--) {
            GDMonoField *field = fields[i];

            if (_get_member_export(field, /* inspect export: */ false, prop_info, exported)) {
                StringName member_name = field->get_name();

                member_info[member_name] = prop_info;
                exported_members_cache.push_front(prop_info);
                exported_members_defval_cache[member_name] = Variant();
            }
        }

        const Vector<GDMonoProperty *> &properties = top->get_all_properties();

        for (int i = properties.size() - 1; i >= 0; i--) {
            GDMonoProperty *property = properties[i];

            if (_get_member_export(property, /* inspect export: */ false, prop_info, exported)) {
                StringName member_name = property->get_name();

                member_info[member_name] = prop_info;
                exported_members_cache.push_front(prop_info);
                exported_members_defval_cache[member_name] = Variant();
            }
        }

        top = top->get_parent_class();
    }
}
#endif

bool CSharpScript::_update_exports(PlaceHolderScriptInstance *p_instance_to_update) {

#ifdef TOOLS_ENABLED
    bool is_editor = Engine::get_singleton()->is_editor_hint();
    if (is_editor) {
        placeholder_fallback_enabled = true; // until proven otherwise
    }
#endif

    if (!valid) {
        return false;
    }

    bool changed = false;

#ifdef TOOLS_ENABLED
    if (exports_invalidated)
#endif
    {
        GD_MONO_SCOPE_THREAD_ATTACH;

        changed = true;

        member_info.clear();

#ifdef TOOLS_ENABLED
        MonoObject *tmp_object = nullptr;
        Object *tmp_native = nullptr;
        uint32_t tmp_pinned_gchandle = 0;

        if (is_editor) {
            exports_invalidated = false;

        exported_members_cache.clear();
        exported_members_defval_cache.clear();

        // Here we create a temporary managed instance of the class to get the initial values
            tmp_object = mono_object_new(mono_domain_get(), script_class->get_mono_ptr());

        if (!tmp_object) {
            ERR_PRINT("Failed to allocate temporary MonoObject.");
            return false;
        }

            tmp_pinned_gchandle = GDMonoUtils::new_strong_gchandle_pinned(tmp_object); // pin it (not sure if needed)

        GDMonoMethod *ctor = script_class->get_method(CACHED_STRING_NAME(dotctor), 0);

        ERR_FAIL_NULL_V_MSG(ctor, false,
                String("Cannot construct temporary MonoObject because the class does not define a parameterless constructor: '") + this->get_path() + "'.");

        MonoException *ctor_exc = nullptr;
        ctor->invoke(tmp_object, nullptr, &ctor_exc);

        tmp_native = GDMonoMarshal::unbox<Object *>(CACHED_FIELD(GodotObject, ptr)->get_value(tmp_object));

        if (ctor_exc) {
            // TODO: Should we free 'tmp_native' if the exception was thrown after its creation?

                GDMonoUtils::free_gchandle(tmp_pinned_gchandle);
            tmp_object = nullptr;

            ERR_PRINT("Exception thrown from constructor of temporary MonoObject:");
            GDMonoUtils::debug_print_unhandled_exception(ctor_exc);
            return false;
        }
        }
#endif

        GDMonoClass *top = script_class;

        while (top && top != native) {
            PropertyInfo prop_info;
            bool exported;

            const Vector<GDMonoField *> &fields = top->get_all_fields();

            for (int i = fields.size() - 1; i >= 0; i--) {
                GDMonoField *field = fields[i];

                if (_get_member_export(field, /* inspect export: */ true, prop_info, exported)) {
                    StringName member_name = field->get_name();

                        member_info[member_name] = prop_info;
                    if (exported) {
#ifdef TOOLS_ENABLED
                        if (is_editor) {
                        exported_members_cache.push_front(prop_info);

                        if (tmp_object) {
                            exported_members_defval_cache[member_name] = GDMonoMarshal::mono_object_to_variant(field->get_value(tmp_object));
                        }
                    }
#endif
#if defined(TOOLS_ENABLED) || defined(DEBUG_ENABLED)
                        exported_members_names.emplace(member_name);
#endif
                    }
                }
            }

            const Vector<GDMonoProperty *> &properties = top->get_all_properties();

            for (int i = properties.size() - 1; i >= 0; i--) {
                GDMonoProperty *property = properties[i];

                if (_get_member_export(property, /* inspect export: */ true, prop_info, exported)) {
                    StringName member_name = property->get_name();

                        member_info[member_name] = prop_info;
                    if (exported) {
#ifdef TOOLS_ENABLED
                        if (is_editor) {
                        exported_members_cache.push_front(prop_info);

                        if (tmp_object) {
                            MonoException *exc = nullptr;
                            MonoObject *ret = property->get_value(tmp_object, &exc);
                            if (exc) {
                                exported_members_defval_cache[member_name] = Variant();
                                GDMonoUtils::debug_print_unhandled_exception(exc);
                            } else {
                                exported_members_defval_cache[member_name] = GDMonoMarshal::mono_object_to_variant(ret);
                            }
                        }
                    }
#endif
#if defined(TOOLS_ENABLED) || defined(DEBUG_ENABLED)
                        exported_members_names.insert(member_name);
#endif
                    }
                }
            }

            top = top->get_parent_class();
        }

#ifdef TOOLS_ENABLED
        if (is_editor) {
        // Need to check this here, before disposal
            bool base_ref = object_cast<RefCounted>(tmp_native) != nullptr;

        // Dispose the temporary managed instance

        MonoException *exc = nullptr;
        GDMonoUtils::dispose(tmp_object, &exc);

        if (exc) {
            ERR_PRINT("Exception thrown from method Dispose() of temporary MonoObject:");
            GDMonoUtils::debug_print_unhandled_exception(exc);
        }

            GDMonoUtils::free_gchandle(tmp_pinned_gchandle);
        tmp_object = nullptr;

        if (tmp_native && !base_ref) {
                Node *node = object_cast<Node>(tmp_native);
            if (node && node->is_inside_tree()) {
                ERR_PRINT("Temporary instance was added to the scene tree.");
            } else {
                memdelete(tmp_native);
            }
        }
    }
#endif
    }

#ifdef TOOLS_ENABLED
    if (is_editor) {
        placeholder_fallback_enabled = false;

        if ((changed || p_instance_to_update) && !placeholders.empty()) {
            // Update placeholders if any
            HashMap<StringName, Variant> values;
            Vector<PropertyInfo> propnames;
            _update_exports_values(values, propnames);

            if (changed) {
                for (PlaceHolderScriptInstance * E : placeholders) {
                    E->update(propnames, values);
                }
            } else {
                p_instance_to_update->update(propnames, values);
            }

        }
    }
#endif

    return changed;
}

void CSharpScript::load_script_signals(GDMonoClass *p_class, GDMonoClass *p_native_class) {
    // no need to load the script's signals more than once
    if (!signals_invalidated) {
        return;
    }

    // make sure this classes signals are empty when loading for the first time
    _signals.clear();
    event_signals.clear();

    GD_MONO_SCOPE_THREAD_ATTACH;

    GDMonoClass *top = p_class;
    while (top && top != p_native_class) {
        const Vector<GDMonoClass *> &delegates = top->get_all_delegates();
        for (int i = delegates.size() - 1; i >= 0; --i) {

            GDMonoClass *delegate = delegates[i];

            if (!delegate->has_attribute(CACHED_CLASS(SignalAttribute))) {
                continue;
            }

            // Arguments are accessibles as arguments of .Invoke method
            GDMonoMethod *invoke_method = delegate->get_method(mono_get_delegate_invoke(delegate->get_mono_ptr()));

            Vector<SignalParameter> parameters;
            if (_get_signal(top, invoke_method, parameters)) {
                _signals[delegate->get_name()] = parameters;
            }
        }

        List<StringName> found_event_signals;

        void *iter = nullptr;
        MonoEvent *raw_event;
        while ((raw_event = mono_class_get_events(top->get_mono_ptr(), &iter)) != nullptr) {
            MonoCustomAttrInfo *event_attrs = mono_custom_attrs_from_event(top->get_mono_ptr(), raw_event);
            if (event_attrs) {
                if (mono_custom_attrs_has_attr(event_attrs, CACHED_CLASS(SignalAttribute)->get_mono_ptr())) {
                    const char *event_name = mono_event_get_name(raw_event);
                    found_event_signals.emplace_back(event_name);
    }

                mono_custom_attrs_free(event_attrs);
            }
}

        const Vector<GDMonoField *> &fields = top->get_all_fields();
        for (int i = 0; i < fields.size(); i++) {
            GDMonoField *field = fields[i];

            GDMonoClass *field_class = field->get_type().type_class;

            if (!mono_class_is_delegate(field_class->get_mono_ptr())) {
                continue;
    }

            if (!found_event_signals.contains(field->get_name())) {
                continue;
            }

            GDMonoMethod *invoke_method = field_class->get_method(mono_get_delegate_invoke(field_class->get_mono_ptr()));

            Vector<SignalParameter> parameters;
            if (_get_signal(top, invoke_method, parameters)) {
                event_signals[field->get_name()] = { field, invoke_method, parameters };
            }
        }

        top = top->get_parent_class();
    }

    signals_invalidated = false;
}

bool CSharpScript::_get_signal(GDMonoClass *p_class, GDMonoMethod *p_delegate_invoke, Vector<SignalParameter> &params) {
    GD_MONO_ASSERT_THREAD_ATTACHED;

    Vector<StringName> names;
    Vector<ManagedType> types;
    p_delegate_invoke->get_parameter_names(names);
    p_delegate_invoke->get_parameter_types(types);

    for (int i = 0; i < names.size(); ++i) {
        SignalParameter arg;
        arg.name = names[i];
        bool nil_is_variant = false;
        arg.type = GDMonoMarshal::managed_to_variant_type(types[i], &nil_is_variant);

        if (arg.type == VariantType::NIL) {
            if (nil_is_variant) {
                arg.nil_is_variant = true;
            } else {
                ERR_PRINT("Unknown type of signal parameter: '" + arg.name + "' in '" + p_class->get_full_name() + "'.");
                return false;
            }
        }

        params.push_back(arg);
    }

    return true;
}

/**
 * Returns false if there was an error, otherwise true.
 * If there was an error, r_prop_info and r_exported are not assigned any value.
 */
bool CSharpScript::_get_member_export(IMonoClassMember *p_member, bool p_inspect_export, PropertyInfo &r_prop_info, bool &r_exported) {

    GD_MONO_ASSERT_THREAD_ATTACHED;

    // Goddammit, C++. All I wanted was some nested functions.
#define MEMBER_FULL_QUALIFIED_NAME(m_member) \
    (m_member->get_enclosing_class()->get_full_name() + "." + (String)m_member->get_name())

    if (p_member->is_static()) {
#ifdef TOOLS_ENABLED
        if (p_member->has_attribute(CACHED_CLASS(ExportAttribute))) {
            ERR_PRINT("Cannot export member because it is static: '" + MEMBER_FULL_QUALIFIED_NAME(p_member) + "'.");
        }
#endif
        return false;
    }

    if (member_info.contains(p_member->get_name())) {
        return false;
    }

    ManagedType type;

    if (p_member->get_member_type() == IMonoClassMember::MEMBER_TYPE_FIELD) {
        type = static_cast<GDMonoField *>(p_member)->get_type();
    } else if (p_member->get_member_type() == IMonoClassMember::MEMBER_TYPE_PROPERTY) {
        type = static_cast<GDMonoProperty *>(p_member)->get_type();
    } else {
        CRASH_NOW();
    }

    bool exported = p_member->has_attribute(CACHED_CLASS(ExportAttribute));

    if (p_member->get_member_type() == IMonoClassMember::MEMBER_TYPE_PROPERTY) {
        GDMonoProperty *property = static_cast<GDMonoProperty *>(p_member);
        if (!property->has_getter()) {
#ifdef TOOLS_ENABLED
            if (exported) {
                ERR_PRINT("Cannot export a property without a getter: '" + MEMBER_FULL_QUALIFIED_NAME(p_member) + "'.");
            }
#endif
            return false;
        }
        if (!property->has_setter()) {
#ifdef TOOLS_ENABLED
            if (exported) {
                ERR_PRINT("Cannot export a property without a setter: '" + MEMBER_FULL_QUALIFIED_NAME(p_member) + "'.");
            }
#endif
            return false;
        }
    }

    bool nil_is_variant = false;
    VariantType variant_type = GDMonoMarshal::managed_to_variant_type(type, &nil_is_variant);

    if (!p_inspect_export || !exported) {
        r_prop_info = PropertyInfo(variant_type, StringName(p_member->get_name()), PropertyHint::None, "", PROPERTY_USAGE_SCRIPT_VARIABLE);
        r_exported = false;
        return true;
    }

#ifdef TOOLS_ENABLED
    MonoObject *attr = p_member->get_attribute(CACHED_CLASS(ExportAttribute));
#endif

    PropertyHint hint = PropertyHint::None;
    String hint_string;

    if (variant_type == VariantType::NIL && !nil_is_variant) {
#ifdef TOOLS_ENABLED
        ERR_PRINT("Unknown exported member type: '" + MEMBER_FULL_QUALIFIED_NAME(p_member) + "'.");
#endif
        return false;
    }

#ifdef TOOLS_ENABLED
    int hint_res = _try_get_member_export_hint(p_member, type, variant_type, /* allow_generics: */ true, hint, hint_string);

    ERR_FAIL_COND_V_MSG(hint_res == -1, false,
            "Error while trying to determine information about the exported member: '" +
                    MEMBER_FULL_QUALIFIED_NAME(p_member) + "'.");

    if (hint_res == 0) {
        hint = PropertyHint(CACHED_FIELD(ExportAttribute, hint)->get_int_value(attr));
        hint_string = CACHED_FIELD(ExportAttribute, hintString)->get_string_value(attr);
    }
#endif

    uint32_t prop_usage = PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SCRIPT_VARIABLE;
    if (variant_type == VariantType::NIL) {
        // System.Object (Variant)
        prop_usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
    }
    r_prop_info = PropertyInfo(variant_type, StringName(p_member->get_name()), hint, hint_string, prop_usage);
    r_exported = true;

    return true;

#undef MEMBER_FULL_QUALIFIED_NAME
}

#ifdef TOOLS_ENABLED
int CSharpScript::_try_get_member_export_hint(IMonoClassMember *p_member, ManagedType p_type,
        VariantType p_variant_type, bool p_allow_generics, PropertyHint &r_hint, String &r_hint_string) {
    if (p_variant_type == VariantType::NIL) {
        // System.Object (Variant)
        return 1;
    }
    GD_MONO_ASSERT_THREAD_ATTACHED;

    if (p_variant_type == VariantType::INT && p_type.type_encoding == MONO_TYPE_VALUETYPE &&
            mono_class_is_enum(p_type.type_class->get_mono_ptr())) {
        MonoReflectionType *reftype = mono_type_get_object(mono_domain_get(), p_type.type_class->get_mono_type());
        r_hint = GDMonoUtils::Marshal::type_has_flags_attribute(reftype) ? PropertyHint::Flags : PropertyHint::Enum;

        Vector<MonoClassField *> fields = p_type.type_class->get_enum_fields();

        MonoType *enum_basetype = mono_class_enum_basetype(p_type.type_class->get_mono_ptr());

        String name_only_hint_string;

        // True: enum Foo { Bar, Baz, Quux }
        // True: enum Foo { Bar = 0, Baz = 1, Quux = 2 }
        // False: enum Foo { Bar = 0, Baz = 7, Quux = 5 }
        bool uses_default_values = true;

        for (int i = 0; i < fields.size(); i++) {
            MonoClassField *field = fields[i];

            if (i > 0) {
                r_hint_string += ",";
                name_only_hint_string += ",";
            }

            String enum_field_name = mono_field_get_name(field);
            r_hint_string += enum_field_name;
            name_only_hint_string += enum_field_name;

            // TODO:
            // Instead of using mono_field_get_value_object, we can do this without boxing. Check the
            // internal mono functions: ves_icall_System_Enum_GetEnumValuesAndNames and the get_enum_field.

            MonoObject *val_obj = mono_field_get_value_object(mono_domain_get(), field, nullptr);

            ERR_FAIL_NULL_V_MSG(val_obj, -1, "Failed to get '" + enum_field_name + "' constant enum value.");

            bool r_error;
            uint64_t val = GDMonoUtils::unbox_enum_value(val_obj, enum_basetype, r_error);
            ERR_FAIL_COND_V_MSG(r_error, -1, "Failed to unbox '" + enum_field_name + "' constant enum value.");

            unsigned int expected_val = r_hint == PropertyHint::Flags ? 1 << i : i;
            if (val != expected_val) {
                uses_default_values = false;
            }

            r_hint_string += ":";
            r_hint_string += StringUtils::num_uint64(val);
        }

        if (uses_default_values) {
            // If we use the format NAME:VAL, that's what the editor displays.
            // That's annoying if the user is not using custom values for the enum constants.
            // This may not be needed in the future if the editor is changed to not display values.
            r_hint_string = name_only_hint_string;
        }
    } else if (p_variant_type == VariantType::OBJECT &&
               CACHED_CLASS(GodotResource)->is_assignable_from(p_type.type_class)) {
        GDMonoClass *field_native_class = GDMonoUtils::get_class_native_base(p_type.type_class);
        CRASH_COND(field_native_class == nullptr);

        r_hint = PropertyHint::ResourceType;
        r_hint_string = NATIVE_GDMONOCLASS_NAME(field_native_class);
    } else if (p_allow_generics && p_variant_type == VariantType::ARRAY) {
        // Nested arrays are not supported in the inspector

        ManagedType elem_type;

        if (!GDMonoMarshal::try_get_array_element_type(p_type, elem_type)) {
            return 0;
        }

        VariantType elem_variant_type = GDMonoMarshal::managed_to_variant_type(elem_type);

        PropertyHint elem_hint = PropertyHint::None;
        String elem_hint_string;

        ERR_FAIL_COND_V_MSG(elem_variant_type == VariantType::NIL, -1, "Unknown array element type.");
        bool preset_hint = false;
        if (elem_variant_type == VariantType::STRING) {
            MonoObject *attr = p_member->get_attribute(CACHED_CLASS(ExportAttribute));
            if (PropertyHint(CACHED_FIELD(ExportAttribute, hint)->get_int_value(attr)) == PropertyHint::Enum) {
                r_hint_string = itos((int)elem_variant_type) + "/" + itos((int)PropertyHint::Enum) + ":" +
                                CACHED_FIELD(ExportAttribute, hintString)->get_string_value(attr);
                preset_hint = true;
            }
        }
        if (!preset_hint) {
            int hint_res = _try_get_member_export_hint(
                    p_member, elem_type, elem_variant_type, /* allow_generics: */ false, elem_hint, elem_hint_string);

            ERR_FAIL_COND_V_MSG(
                    hint_res == -1, -1, "Error while trying to determine information about the array element type.");

            // Format: type/hint:hint_string
            r_hint_string = itos((int)elem_variant_type) + "/" + itos((int)elem_hint) + ":" + elem_hint_string;
        }
        r_hint = PropertyHint::TypeString;

    } else if (p_allow_generics && p_variant_type == VariantType::DICTIONARY) {
        // TODO: Dictionaries are not supported in the inspector
    } else {
        return 0;
    }

    return 1;
}
#endif


Variant CSharpScript::call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    if (unlikely(GDMono::get_singleton() == nullptr)) {
        // Probably not the best error but eh.
        r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
        return Variant();
    }

    GD_MONO_SCOPE_THREAD_ATTACH;

    GDMonoClass *top = script_class;

    while (top && top != native) {
        GDMonoMethod *method = top->get_method(p_method, p_argcount);

        if (method && method->is_static()) {
            MonoObject *result = method->invoke(nullptr, p_args);

            if (result) {
                return GDMonoMarshal::mono_object_to_variant(result);
            } else {
                return Variant();
            }
        }

        top = top->get_parent_class();
    }

    // No static method found. Try regular instance calls
    return Script::call(p_method, p_args, p_argcount, r_error);
}

void CSharpScript::_resource_path_changed() {

    _update_name();
}

bool CSharpScript::_get(const StringName &p_name, Variant &r_ret) const {

    if (p_name == CSharpLanguage::singleton->string_names._script_source) {

        r_ret = get_source_code();
        return true;
    }

    return false;
}

bool CSharpScript::_set(const StringName &p_name, const Variant &p_value) {

    if (p_name == CSharpLanguage::singleton->string_names._script_source) {

        set_source_code(p_value.as<String>());
        reload();
        return true;
    }

    return false;
}

void CSharpScript::_get_property_list(Vector<PropertyInfo> *p_properties) const {
    p_properties->emplace_back(VariantType::STRING, StringName(CSharpLanguage::singleton->string_names._script_source),
            PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
}

void CSharpScript::_bind_methods() {

    MethodBinder::bind_vararg_method("new", &CSharpScript::_new, MethodInfo("new"));
}

Ref<CSharpScript> CSharpScript::create_for_managed_type(GDMonoClass *p_class, GDMonoClass *p_native) {

    // This method should not fail, only assertions allowed

    CRASH_COND(p_class == nullptr);

    // TODO OPTIMIZE: Cache the 'CSharpScript' associated with this 'p_class' instead of allocating a new one every time
    Ref<CSharpScript> script(memnew(CSharpScript), DoNotAddRef);

    initialize_for_managed_type(script, p_class, p_native);

    return script;
}

void CSharpScript::initialize_for_managed_type(Ref<CSharpScript> p_script, GDMonoClass *p_class, GDMonoClass *p_native) {

    // This method should not fail, only assertions allowed

    CRASH_COND(p_class == nullptr);

    p_script->name = p_class->get_name();
    p_script->script_class = p_class;
    p_script->native = p_native;

    CRASH_COND(p_script->native == nullptr);

    GDMonoClass *base = p_script->script_class->get_parent_class();

    // `base` should only be set if the script is a user defined type.
    if (base != p_script->native) {
        p_script->base = base;
    }

    p_script->valid = true;
    p_script->tool = p_script->script_class->has_attribute(CACHED_CLASS(ToolAttribute));
    p_script->reload_invalidated = false;

    if (!p_script->tool) {
        GDMonoClass *nesting_class = p_script->script_class->get_nesting_class();
        p_script->tool = nesting_class && nesting_class->has_attribute(CACHED_CLASS(ToolAttribute));
    }

#ifdef TOOLS_ENABLED
    if (!p_script->tool) {
        p_script->tool = p_script->script_class->get_assembly() == GDMono::get_singleton()->get_tools_assembly();
    }
#endif

#ifdef DEBUG_ENABLED
    // For debug builds, we must fetch from all native base methods as well.
    // Native base methods must be fetched before the current class.
    // Not needed if the script class itself is a native class.

    if (p_script->script_class != p_script->native) {
        GDMonoClass *native_top = p_script->native;
        while (native_top) {
            native_top->fetch_methods_with_godot_api_checks(p_script->native);

            if (native_top == CACHED_CLASS(GodotObject)) {
                break;
            }

            native_top = native_top->get_parent_class();
        }
    }
#endif

    p_script->script_class->fetch_methods_with_godot_api_checks(p_script->native);

    // Need to fetch method from base classes as well
    GDMonoClass *top = p_script->script_class;
    while (top && top != p_script->native) {
        // Fetch methods from base classes as well
        top->fetch_methods_with_godot_api_checks(p_script->native);
        top = top->get_parent_class();
    }

    p_script->load_script_signals(p_script->script_class, p_script->native);
#ifdef TOOLS_ENABLED
    p_script->_update_member_info_no_exports();
#endif
}

bool CSharpScript::can_instance() const {

#ifdef TOOLS_ENABLED
    bool extra_cond = tool || ScriptServer::is_scripting_enabled();
#else
    bool extra_cond = true;
#endif

    // FIXME Need to think this through better.
    // For tool scripts, this will never fire if the class is not found. That's because we
    // don't know if it's a tool script if we can't find the class to access the attributes.
    if (extra_cond && !script_class) {
        if (GDMono::get_singleton()->get_project_assembly() == nullptr) {
            // The project assembly is not loaded
            ERR_FAIL_V_MSG(false, "Cannot instance script because the project assembly is not loaded. Script: '" + get_path() + "'.");
        } else {
            // The project assembly is loaded, but the class could not found
            ERR_FAIL_V_MSG(false, "Cannot instance script because the class '" + name + "' could not be found. Script: '" + get_path() + "'.");
        }
    }

    return valid && extra_cond;
}

StringName CSharpScript::get_instance_base_type() const {

    if (native) {
        return native->get_name();
    } else {
        return StringName();
    }
}

CSharpInstance *CSharpScript::_create_instance(const Variant **p_args, int p_argcount, Object *p_owner, bool p_isref, Callable::CallError &r_error) {
    GD_MONO_ASSERT_THREAD_ATTACHED;

    /* STEP 1, CREATE */

    // Search the constructor first, to fail with an error if it's not found before allocating anything else.
    GDMonoMethod *ctor = script_class->get_method(CACHED_STRING_NAME(dotctor), p_argcount);
    if (ctor == nullptr) {
        ERR_FAIL_COND_V_MSG(p_argcount == 0, nullptr,
                "Cannot create script instance. The class '" + script_class->get_full_name() +
                        "' does not define a parameterless constructor." +
                        (get_path().empty() ? String() : " Path: '" + get_path() + "'."));

        ERR_FAIL_V_MSG(nullptr, "Constructor not found.");
    }

    Ref<RefCounted> ref;
    if (p_isref) {
        // Hold it alive. Important if we have to dispose a script instance binding before creating the CSharpInstance.
        ref = Ref<RefCounted>(static_cast<RefCounted *>(p_owner));
    }

    // If the object had a script instance binding, dispose it before adding the CSharpInstance
    if (p_owner->has_script_instance_binding(CSharpLanguage::get_singleton()->get_language_index())) {
        void *data = p_owner->get_script_instance_binding(CSharpLanguage::get_singleton()->get_language_index());
        CRASH_COND(data == nullptr);
        Map<Object *, CSharpScriptBinding>::iterator iter;
        iter.mpNode = (Map<Object *, CSharpScriptBinding>::iterator::node_type *)data;
        CSharpScriptBinding &script_binding = iter->second;

        if (script_binding.inited && !script_binding.gchandle.is_released()) {
            MonoObject *mono_object = script_binding.gchandle.get_target();
            if (mono_object) {
                MonoException *exc = nullptr;
                GDMonoUtils::dispose(mono_object, &exc);

                if (exc) {
                    GDMonoUtils::set_pending_exception(exc);
                }
            }

            script_binding.gchandle.release(); // Just in case
            script_binding.inited = false;
        }
    }

    CSharpInstance *instance = memnew(CSharpInstance(Ref<CSharpScript>(this)));
    instance->base_ref = p_isref;
    instance->owner = p_owner;
    instance->owner->set_script_instance(instance);

    /* STEP 2, INITIALIZE AND CONSTRUCT */

    MonoObject *mono_object = mono_object_new(mono_domain_get(), script_class->get_mono_ptr());

    if (!mono_object) {
        // Important to clear this before destroying the script instance here
        instance->script = Ref<CSharpScript>();
        instance->owner = nullptr;

        bool die = instance->_unreference_owner_unsafe();
        // Not ok for the owner to die here. If there is a situation where this can happen, it will be considered a bug.
        CRASH_COND(die);

        p_owner->set_script_instance(nullptr);
        r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
        ERR_FAIL_V_MSG(nullptr, "Failed to allocate memory for the object.");
    }

    // Tie managed to unmanaged
    instance->gchandle = MonoGCHandleData::new_strong_handle(mono_object);

    if (instance->base_ref) {
        instance->_reference_owner_unsafe(); // Here, after assigning the gchandle (for the refcount_incremented callback)
    }

    {
        MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);
        instances.insert(instance->owner);
    }

    CACHED_FIELD(GodotObject, ptr)->set_value_raw(mono_object, instance->owner);

    // Construct
    ctor->invoke(mono_object, p_args);

    /* STEP 3, PARTY */

    //@TODO make thread safe
    return instance;
}

Variant CSharpScript::_new(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    if (!valid) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
        return Variant();
    }

    r_error.error = Callable::CallError::CALL_OK;

    ERR_FAIL_NULL_V(native, Variant());

    GD_MONO_SCOPE_THREAD_ATTACH;

    Object *owner = ClassDB::instance(StringName(NATIVE_GDMONOCLASS_NAME(native)));

    REF ref;
    RefCounted *r = object_cast<RefCounted>(owner);
    if (r) {
        ref = REF(r, DoNotAddRef);
    }

    CSharpInstance *instance = _create_instance(p_args, p_argcount, owner, r != nullptr, r_error);
    if (!instance) {
        if (!ref) {
            memdelete(owner); //no owner, sorry
        }
        return Variant();
    }

    if (ref) {
        return ref;
    } else {
        return Variant::from(owner);
    }
}

ScriptInstance *CSharpScript::instance_create(Object *p_this) {

#ifdef DEBUG_ENABLED
    CRASH_COND(!valid);
#endif
    GD_MONO_SCOPE_THREAD_ATTACH;

    if (native) {
        StringName native_name(NATIVE_GDMONOCLASS_NAME(native));
        if (!ClassDB::is_parent_class(p_this->get_class_name(), native_name)) {
            if (ScriptDebugger::get_singleton()) {
                CSharpLanguage::get_singleton()->debug_break_parse(get_path(), 0,
                        String("Script inherits from native type '") + native_name +
                                "', so it can't be instanced in object of type: '" + p_this->get_class() + "'");
            }
            ERR_FAIL_V_MSG(nullptr, String("Script inherits from native type '") + native_name +
                                         "', so it can't be instanced in object of type: '" + p_this->get_class() +
                                         "'.");
        }
    }

    Callable::CallError unchecked_error;
    return _create_instance(nullptr, 0, p_this, object_cast<RefCounted>(p_this) != nullptr, unchecked_error);
}

PlaceHolderScriptInstance *CSharpScript::placeholder_instance_create(Object *p_this) {

#ifdef TOOLS_ENABLED
    PlaceHolderScriptInstance *si = memnew(PlaceHolderScriptInstance(CSharpLanguage::get_singleton(), Ref<Script>(this), p_this));
    placeholders.insert(si);
    _update_exports(si);
    return si;
#else
    return nullptr;
#endif
}

bool CSharpScript::instance_has(Object *p_this) const {

    MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);
    return instances.contains(const_cast<Object *>(p_this));
}

bool CSharpScript::has_source_code() const {

    return !source.empty();
}

StringView CSharpScript::get_source_code() const {

    return source;
}

void CSharpScript::set_source_code(String p_code) {

    if (source == p_code) {
        return;
    }
    source = eastl::move(p_code);
#ifdef TOOLS_ENABLED
    source_changed_cache = true;
#endif
}

void CSharpScript::get_script_method_list(Vector<MethodInfo> *p_list) const {

    if (!script_class) {
        return;
    }

    GD_MONO_SCOPE_THREAD_ATTACH;

    // TODO: We're filtering out constructors but there may be other methods unsuitable for explicit calls.
    GDMonoClass *top = script_class;

    while (top && top != native) {
        const Vector<GDMonoMethod *> &methods = top->get_all_methods();
        for (int i = 0; i < methods.size(); ++i) {
            MethodInfo minfo = methods[i]->get_method_info();
            if (minfo.name != CACHED_STRING_NAME(dotctor)) {
                p_list->push_back(methods[i]->get_method_info());
            }
        }

        top = top->get_parent_class();
    }
}

bool CSharpScript::has_method(const StringName &p_method) const {

    if (!script_class) {
        return false;
    }

    GD_MONO_SCOPE_THREAD_ATTACH;

    return script_class->has_fetched_method_unknown_params(p_method);
}

MethodInfo CSharpScript::get_method_info(const StringName &p_method) const {

    if (!script_class) {
        return MethodInfo();
    }

    GD_MONO_SCOPE_THREAD_ATTACH;

    GDMonoClass *top = script_class;

    while (top && top != native) {
        GDMonoMethod *params = top->get_fetched_method_unknown_params(p_method);
        if (params) {
            return params->get_method_info();
        }

        top = top->get_parent_class();
    }

    return MethodInfo();
}

Error CSharpScript::reload(bool p_keep_state) {
    if (!reload_invalidated) {
        return OK;
    }
    // In the case of C#, reload doesn't really do any script reloading.
    // That's done separately via domain reloading.
    reload_invalidated = false;

    GD_MONO_SCOPE_THREAD_ATTACH;

    GDMonoAssembly *project_assembly = GDMono::get_singleton()->get_project_assembly();

    if (!project_assembly) return ERR_FILE_MISSING_DEPENDENCIES;

    const Variant *script_metadata_var = CSharpLanguage::get_singleton()->get_scripts_metadata().getptr(StringName(get_path()));
    if (script_metadata_var) {
        Dictionary script_metadata = script_metadata_var->as<Dictionary>()["class"].as<Dictionary>();
        const Variant *namespace_ = script_metadata.getptr("namespace");
        const Variant *class_name = script_metadata.getptr("class_name");
        ERR_FAIL_NULL_V(namespace_, ERR_BUG);
        ERR_FAIL_NULL_V(class_name, ERR_BUG);
        GDMonoClass *klass =
                project_assembly->get_class(StringName(namespace_->as<String>()), StringName(class_name->as<String>()));
        if (klass && CACHED_CLASS(GodotObject)->is_assignable_from(klass)) {
            script_class = klass;
        }
    } else {
        // Missing script metadata. Fallback to legacy method
        script_class = project_assembly->get_object_derived_class(name);
    }

    valid = script_class != nullptr;

    if (script_class) {
#ifdef DEBUG_ENABLED
        print_verbose("Found class " + script_class->get_full_name() + " for script " + get_path());
#endif

        tool = script_class->has_attribute(CACHED_CLASS(ToolAttribute));

        if (!tool) {
            GDMonoClass *nesting_class = script_class->get_nesting_class();
            tool = nesting_class && nesting_class->has_attribute(CACHED_CLASS(ToolAttribute));
        }

#if TOOLS_ENABLED
        if (!tool) {
            tool = script_class->get_assembly() == GDMono::get_singleton()->get_tools_assembly();
        }
#endif

        native = GDMonoUtils::get_class_native_base(script_class);

        CRASH_COND(native == nullptr);

        GDMonoClass *base_class = script_class->get_parent_class();

        if (base_class != native) {
            base = base_class;
        }

#ifdef DEBUG_ENABLED
        // For debug builds, we must fetch from all native base methods as well.
        // Native base methods must be fetched before the current class.
        // Not needed if the script class itself is a native class.

        if (script_class != native) {
            GDMonoClass *native_top = native;
            while (native_top) {
                native_top->fetch_methods_with_godot_api_checks(native);

                if (native_top == CACHED_CLASS(GodotObject)) {
                    break;
                }

                native_top = native_top->get_parent_class();
            }
        }
#endif

        script_class->fetch_methods_with_godot_api_checks(native);

        // Need to fetch method from base classes as well
        GDMonoClass *top = script_class;
        while (top && top != native) {
            top->fetch_methods_with_godot_api_checks(native);
            top = top->get_parent_class();
        }

        load_script_signals(script_class, native);
        _update_exports();
    }

    rpc_functions.clear();
    rpc_variables.clear();

    GDMonoClass *top = script_class;
    while (top && top != native) {
        {
            Vector<GDMonoMethod *> methods = top->get_all_methods();
            for (int i = 0; i < methods.size(); i++) {
                if (!methods[i]->is_static()) {
                    MultiplayerAPI_RPCMode mode = _member_get_rpc_mode(methods[i]);
                    if (MultiplayerAPI_RPCMode::RPC_MODE_DISABLED != mode) {
                        ScriptNetData nd;
                        nd.name = methods[i]->get_name();
                        nd.mode = mode;
                        if (!rpc_functions.contains(nd)) {
                            rpc_functions.push_back(nd);
                        }
                    }
                }
            }
        }

        {
            Vector<GDMonoField *> fields = top->get_all_fields();
            for (int i = 0; i < fields.size(); i++) {
                if (!fields[i]->is_static()) {
                    MultiplayerAPI_RPCMode mode = _member_get_rpc_mode(fields[i]);
                    if (MultiplayerAPI_RPCMode::RPC_MODE_DISABLED != mode) {
                        ScriptNetData nd;
                        nd.name = fields[i]->get_name();
                        nd.mode = mode;
                        if (!rpc_variables.contains(nd)) {
                            rpc_variables.push_back(nd);
                        }
                    }
                }
            }
        }

        {
            Vector<GDMonoProperty *> properties = top->get_all_properties();
            for (int i = 0; i < properties.size(); i++) {
                if (!properties[i]->is_static()) {
                    MultiplayerAPI_RPCMode mode = _member_get_rpc_mode(properties[i]);
                    if (MultiplayerAPI_RPCMode::RPC_MODE_DISABLED != mode) {
                        ScriptNetData nd;
                        nd.name = properties[i]->get_name();
                        nd.mode = mode;
                        if (!rpc_variables.contains(nd)) {
                            rpc_variables.push_back(nd);
                        }
                    }
                }
            }
        }

        top = top->get_parent_class();
    }

    // Sort so we are 100% that they are always the same.
    SortNetData sorter;
    eastl::sort(rpc_functions.begin(), rpc_functions.end(), sorter);
    eastl::sort(rpc_variables.begin(), rpc_variables.end(), sorter);
    return OK;
}

ScriptLanguage *CSharpScript::get_language() const {

    return CSharpLanguage::get_singleton();
}

bool CSharpScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {

#ifdef TOOLS_ENABLED

    auto E = exported_members_defval_cache.find(p_property);
    if (E!=exported_members_defval_cache.end()) {
        r_value = E->second;
        return true;
    }

    if (base_cache) {
        return base_cache->get_property_default_value(p_property, r_value);
    }

#endif
    return false;
}

void CSharpScript::update_exports() {

#ifdef TOOLS_ENABLED
    _update_exports();
#endif
}

bool CSharpScript::has_script_signal(const StringName &p_signal) const {
    return _signals.contains(p_signal) || event_signals.contains(p_signal);
}

void CSharpScript::get_script_signal_list(Vector<MethodInfo> *r_signals) const {
    for (const auto & E : _signals) {
        MethodInfo mi(E.first);

        const Vector<SignalParameter> &params = E.second;
        for (const SignalParameter& param : params) {
            PropertyInfo arg_info = PropertyInfo(param.type, StringName(param.name));
            if (param.type == VariantType::NIL && param.nil_is_variant) {
                arg_info.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
        }
            mi.arguments.push_back(arg_info);
        }
        r_signals->emplace_back(eastl::move(mi));
    }
    for (const auto &E : event_signals) {
        MethodInfo mi(E.first);

        const EventSignal &event_signal = E.second;
        const Vector<SignalParameter> &params = event_signal.parameters;
        for (int i = 0; i < params.size(); i++) {
            const SignalParameter &param = params[i];

            PropertyInfo arg_info = PropertyInfo(param.type, StringName(param.name));
            if (param.type == VariantType::NIL && param.nil_is_variant) {
                arg_info.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
            }

            mi.arguments.push_back(arg_info);
        }

        r_signals->emplace_back(eastl::move(mi));
    }
}

bool CSharpScript::inherits_script(const Ref<Script> &p_script) const {
    Ref<CSharpScript> cs = dynamic_ref_cast<CSharpScript>(p_script);
    if (!cs) {
        return false;
    }

    if (script_class == nullptr || cs->script_class == nullptr) {
        return false;
    }

    if (script_class == cs->script_class) {
        return true;
    }

    return cs->script_class->is_assignable_from(script_class);
}

Ref<Script> CSharpScript::get_base_script() const {

    // TODO search in metadata file once we have it, not important any way?
    return Ref<Script>();
}

void CSharpScript::get_script_property_list(Vector<PropertyInfo> *p_list) const {

    for (const auto &E : member_info) {
        p_list->push_back(E.second);
    }
}

int CSharpScript::get_member_line(const StringName &p_member) const {

    // TODO omnisharp
    return -1;
}

MultiplayerAPI_RPCMode CSharpScript::_member_get_rpc_mode(IMonoClassMember *p_member) const {
    if (p_member->has_attribute(CACHED_CLASS(RemoteAttribute))) {
        return MultiplayerAPI_RPCMode::RPC_MODE_REMOTE;
    }
    if (p_member->has_attribute(CACHED_CLASS(MasterAttribute))) {
        return MultiplayerAPI_RPCMode::RPC_MODE_MASTER;
    }
    if (p_member->has_attribute(CACHED_CLASS(PuppetAttribute))) {
        return MultiplayerAPI_RPCMode::RPC_MODE_PUPPET;
    }
    if (p_member->has_attribute(CACHED_CLASS(RemoteSyncAttribute))) {
        return MultiplayerAPI_RPCMode::RPC_MODE_REMOTESYNC;
    }
    if (p_member->has_attribute(CACHED_CLASS(MasterSyncAttribute))) {
        return MultiplayerAPI_RPCMode::RPC_MODE_MASTERSYNC;
    }
    if (p_member->has_attribute(CACHED_CLASS(PuppetSyncAttribute))) {
        return MultiplayerAPI_RPCMode::RPC_MODE_PUPPETSYNC;
    }

    return MultiplayerAPI_RPCMode::RPC_MODE_DISABLED;
}

Vector<ScriptNetData> CSharpScript::get_rpc_methods() const {
    return rpc_functions;
}

uint16_t CSharpScript::get_rpc_method_id(const StringName &p_method) const {
    for (int i = 0; i < rpc_functions.size(); i++) {
        if (rpc_functions[i].name == p_method) {
            return i;
        }
    }
    return UINT16_MAX;
}

StringName CSharpScript::get_rpc_method(const uint16_t p_rpc_method_id) const {
    ERR_FAIL_COND_V(p_rpc_method_id >= rpc_functions.size(), StringName());
    return rpc_functions[p_rpc_method_id].name;
}

MultiplayerAPI_RPCMode CSharpScript::get_rpc_mode_by_id(const uint16_t p_rpc_method_id) const {
    ERR_FAIL_COND_V(p_rpc_method_id >= rpc_functions.size(), MultiplayerAPI_RPCMode::RPC_MODE_DISABLED);
    return rpc_functions[p_rpc_method_id].mode;
}

MultiplayerAPI_RPCMode CSharpScript::get_rpc_mode(const StringName &p_method) const {
    return get_rpc_mode_by_id(get_rpc_method_id(p_method));
}

Vector<ScriptNetData> CSharpScript::get_rset_properties() const {
    return rpc_variables;
}

uint16_t CSharpScript::get_rset_property_id(const StringName &p_variable) const {
    for (int i = 0; i < rpc_variables.size(); i++) {
        if (rpc_variables[i].name == p_variable) {
            return i;
        }
    }
    return UINT16_MAX;
}

StringName CSharpScript::get_rset_property(const uint16_t p_rset_member_id) const {
    ERR_FAIL_COND_V(p_rset_member_id >= rpc_variables.size(), StringName());
    return rpc_variables[p_rset_member_id].name;
}

MultiplayerAPI_RPCMode CSharpScript::get_rset_mode_by_id(const uint16_t p_rset_member_id) const {
    ERR_FAIL_COND_V(p_rset_member_id >= rpc_functions.size(), MultiplayerAPI_RPCMode::RPC_MODE_DISABLED);
    return rpc_functions[p_rset_member_id].mode;
}

MultiplayerAPI_RPCMode CSharpScript::get_rset_mode(const StringName &p_variable) const {
    return get_rset_mode_by_id(get_rset_property_id(p_variable));
}

Error CSharpScript::load_source_code(StringView p_path) {
    Error ferr = read_all_file_utf8(p_path, source);

    ERR_FAIL_COND_V_MSG(ferr != OK, ferr,
            ferr == ERR_INVALID_DATA ?
                    "Script '" + p_path + "' contains invalid unicode (UTF-8), so it was not loaded."
                                          " Please ensure that scripts are saved in valid UTF-8 unicode." :
                    "Failed to read file: '" + p_path + "'.");

#ifdef TOOLS_ENABLED
    source_changed_cache = true;
#endif

    return OK;
}

void CSharpScript::_update_name() {
    String path = get_path();

    if (!path.empty()) {
        name = StringName(PathUtils::get_basename(PathUtils::get_file(get_path())));
    }
}
void CSharpScript::_clear() {
    tool = false;
    valid = false;
    reload_invalidated = true;

    base = nullptr;
    native = nullptr;
    script_class = nullptr;
}

CSharpScript::CSharpScript() {
    _clear();

    _update_name();

#ifdef DEBUG_ENABLED
    {
        MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);
        CSharpLanguage::get_singleton()->script_list.push_back(this);
    }
#endif
}

CSharpScript::~CSharpScript() {

#ifdef DEBUG_ENABLED
    MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);
    CSharpLanguage::get_singleton()->script_list.erase_first(this);
#endif
}
void CSharpScript::get_members(HashSet<StringName> *p_members) {
#if defined(TOOLS_ENABLED) || defined(DEBUG_ENABLED)
    if (p_members) {
        p_members->insert(exported_members_names.begin(),exported_members_names.end());
    }
#endif
}
/*************** RESOURCE ***************/

RES ResourceFormatLoaderCSharpScript::load(StringView p_path, StringView p_original_path, Error *r_error, bool p_no_subresource_cache) {

    if (r_error) {
        *r_error = ERR_FILE_CANT_OPEN;
    }
    // TODO ignore anything inside bin/ and obj/ in tools builds?

    CSharpScript *script = memnew(CSharpScript);

    Ref<CSharpScript> scriptres(script);

#if defined(DEBUG_ENABLED) || defined(TOOLS_ENABLED)
    Error err = script->load_source_code(p_path);
    ERR_FAIL_COND_V_MSG(err != OK, RES(), "Cannot load C# script file '" + p_path + "'.");
#endif

    script->set_path(p_original_path);

    script->reload();

    if (r_error) {
        *r_error = OK;
    }

    return scriptres;
}

void ResourceFormatLoaderCSharpScript::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.push_back("cs");
}

bool ResourceFormatLoaderCSharpScript::handles_type(StringView p_type) const {

    return p_type == StringView("Script") || p_type == StringView(CSharpLanguage::get_singleton()->get_type());
}

String ResourceFormatLoaderCSharpScript::get_resource_type(StringView p_path) const {

    return StringUtils::to_lower(PathUtils::get_extension(p_path)) == "cs" ? CSharpLanguage::get_singleton()->get_type() : "";
}

Error ResourceFormatSaverCSharpScript::save(StringView p_path, const RES &p_resource, uint32_t p_flags) {

    Ref<CSharpScript> sqscr = dynamic_ref_cast<CSharpScript>(p_resource);
    ERR_FAIL_COND_V(!sqscr, ERR_INVALID_PARAMETER);

    StringView source = sqscr->get_source_code();

#ifdef TOOLS_ENABLED
    if (!FileAccess::exists(p_path)) {
        // The file does not yet exists, let's assume the user just created this script

        if (_create_project_solution_if_needed()) {
            CSharpProject::add_item(GodotSharpDirs::get_project_csproj_path(),
                    "Compile",
                    ProjectSettings::get_singleton()->globalize_path(p_path));
        } else {
            ERR_PRINT("C# project could not be created; cannot add file: '" + p_path + "'.");
        }
    }
#endif

    Error err;
    FileAccess *file = FileAccess::open(p_path, FileAccess::WRITE, &err);
    ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save C# script file '" + p_path + "'.");

    file->store_string(source);

    if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
        memdelete(file);
        return ERR_CANT_CREATE;
    }

    file->close();
    memdelete(file);

#ifdef TOOLS_ENABLED
    if (ScriptServer::is_reload_scripts_on_save_enabled()) {
        CSharpLanguage::get_singleton()->reload_tool_script(sqscr, false);
    }
#endif

    return OK;
}

void ResourceFormatSaverCSharpScript::get_recognized_extensions(const RES &p_resource, Vector<String> &p_extensions) const {

    if (object_cast<CSharpScript>(p_resource.get())) {
        p_extensions.push_back("cs");
    }
}

bool ResourceFormatSaverCSharpScript::recognize(const RES &p_resource) const {

    return object_cast<CSharpScript>(p_resource.get()) != nullptr;
}

CSharpLanguage::StringNameCache::StringNameCache() {

    _signal_callback = StringName("_signal_callback");
    _set = StringName("_set");
    _get = StringName("_get");
    _get_property_list = StringName("_get_property_list");
    _notification = StringName("_notification");
    _script_source = StringName("script/source");
    on_before_serialize = StringName("OnBeforeSerialize");
    on_after_deserialize = StringName("OnAfterDeserialize");
    dotctor = StringName(".ctor");
    delegate_invoke_method_name = StringName("Invoke");
}
