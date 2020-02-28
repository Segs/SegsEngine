/*************************************************************************/
/*  script_language.cpp                                                  */
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

#include "script_language.h"

#include "core/core_string_names.h"
#include "core/project_settings.h"
#include "core/object_tooling.h"
#include "core/method_bind.h"

#include "EASTL/sort.h"

IMPL_GDCLASS(Script)

ScriptLanguage *ScriptServer::_languages[MAX_LANGUAGES];
int ScriptServer::_language_count = 0;

bool ScriptServer::scripting_enabled = true;
bool ScriptServer::reload_scripts_on_save = false;
bool ScriptServer::languages_finished = false;
ScriptEditRequestFunction ScriptServer::edit_request_func = nullptr;

void Script::_notification(int p_what) {

    if (p_what == NOTIFICATION_POSTINITIALIZE) {

        if (ScriptDebugger::get_singleton())
            ScriptDebugger::get_singleton()->set_break_language(get_language());
    }
}

Variant Script::_get_property_default_value(const StringName &p_property) {
    Variant ret;
    get_property_default_value(p_property, ret);
    return ret;
}

Array Script::_get_script_property_list() {
    Array ret;
    Vector<PropertyInfo> list;
    get_script_property_list(&list);
    for(PropertyInfo &E : list ) {
        ret.append(E.operator Dictionary());
    }
    return ret;
}

Array Script::_get_script_method_list() {
    Array ret;
    Vector<MethodInfo> list;
    get_script_method_list(&list);
    for(MethodInfo &E : list ) {
        ret.append(E.operator Dictionary());
    }
    return ret;
}

Array Script::_get_script_signal_list() {
    Array ret;
    Vector<MethodInfo> list;
    get_script_signal_list(&list);
    for(MethodInfo &E : list ) {
        ret.append(E.operator Dictionary());
    }
    return ret;
}

Dictionary Script::_get_script_constant_map() {
    Dictionary ret;
    HashMap<StringName, Variant> map;
    get_constants(&map);
    for (const eastl::pair<const StringName,Variant> &E : map) {
        ret[E.first] = E.second;
    }
    return ret;
}
void Script::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("can_instance"), &Script::can_instance);
    //MethodBinder::bind_method(D_METHOD("instance_create","base_object"),&Script::instance_create);
    MethodBinder::bind_method(D_METHOD("instance_has", {"base_object"}), &Script::instance_has);
    MethodBinder::bind_method(D_METHOD("has_source_code"), &Script::has_source_code);
    MethodBinder::bind_method(D_METHOD("get_source_code"), &Script::get_source_code);
    MethodBinder::bind_method(D_METHOD("set_source_code", {"source"}), &Script::set_source_code);
    MethodBinder::bind_method(D_METHOD("reload", {"keep_state"}), &Script::reload, {DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("get_base_script"), &Script::get_base_script);
    MethodBinder::bind_method(D_METHOD("get_instance_base_type"), &Script::get_instance_base_type);

    MethodBinder::bind_method(D_METHOD("has_script_signal", {"signal_name"}), &Script::has_script_signal);

    MethodBinder::bind_method(D_METHOD("get_script_property_list"), &Script::_get_script_property_list);
    MethodBinder::bind_method(D_METHOD("get_script_method_list"), &Script::_get_script_method_list);
    MethodBinder::bind_method(D_METHOD("get_script_signal_list"), &Script::_get_script_signal_list);
    MethodBinder::bind_method(D_METHOD("get_script_constant_map"), &Script::_get_script_constant_map);
    MethodBinder::bind_method(D_METHOD("get_property_default_value", {"property"}), &Script::_get_property_default_value);

    MethodBinder::bind_method(D_METHOD("is_tool"), &Script::is_tool);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "source_code", PropertyHint::None, "", 0), "set_source_code", "get_source_code");
}

void ScriptServer::set_scripting_enabled(bool p_enabled) {

    scripting_enabled = p_enabled;
}

bool ScriptServer::is_scripting_enabled() {

    return scripting_enabled;
}

ScriptLanguage *ScriptServer::get_language(int p_idx) {

    ERR_FAIL_INDEX_V(p_idx, _language_count, nullptr);

    return _languages[p_idx];
}

void ScriptServer::register_language(ScriptLanguage *p_language) {

    ERR_FAIL_COND(_language_count >= MAX_LANGUAGES);
    _languages[_language_count++] = p_language;
}

void ScriptServer::unregister_language(ScriptLanguage *p_language) {

    for (int i = 0; i < _language_count; i++) {
        if (_languages[i] == p_language) {
            _language_count--;
            if (i < _language_count) {
                SWAP(_languages[i], _languages[_language_count]);
            }
            return;
        }
    }
}

void ScriptServer::init_languages() {

    { //load global classes
        global_classes_clear();
        if (ProjectSettings::get_singleton()->has_setting("_global_script_classes")) {
            Array script_classes = ProjectSettings::get_singleton()->get(StaticCString("_global_script_classes"));

            for (int i = 0; i < script_classes.size(); i++) {
                Dictionary c = script_classes[i];
                if (!c.has("class") || !c.has("language") || !c.has("path") || !c.has("base"))
                    continue;
                add_global_class(c["class"], c["base"], c["language"], c["path"].as<String>());
            }
        }
    }

    for (int i = 0; i < _language_count; i++) {
        _languages[i]->init();
    }
}

void ScriptServer::finish_languages() {

    for (int i = 0; i < _language_count; i++) {
        _languages[i]->finish();
    }
    global_classes_clear();
    languages_finished = true;
}

void ScriptServer::set_reload_scripts_on_save(bool p_enable) {

    reload_scripts_on_save = p_enable;
}

bool ScriptServer::is_reload_scripts_on_save_enabled() {

    return reload_scripts_on_save;
}

void ScriptServer::thread_enter() {

    for (int i = 0; i < _language_count; i++) {
        _languages[i]->thread_enter();
    }
}

void ScriptServer::thread_exit() {

    for (int i = 0; i < _language_count; i++) {
        _languages[i]->thread_exit();
    }
}

HashMap<StringName, ScriptServer::GlobalScriptClass> ScriptServer::global_classes;

void ScriptServer::global_classes_clear() {
    global_classes.clear();
}

void ScriptServer::add_global_class(const StringName &p_class, const StringName &p_base, const StringName &p_language, StringView p_path) {
    ERR_FAIL_COND_MSG(p_class == p_base || (global_classes.contains(p_base) && get_global_class_native_base(p_base) == p_class), "Cyclic inheritance in script class.");
    GlobalScriptClass g;
    g.language = p_language;
    g.path = p_path;
    g.base = p_base;
    global_classes[p_class] = g;
}
void ScriptServer::remove_global_class(const StringName &p_class) {
    global_classes.erase(p_class);
}
bool ScriptServer::is_global_class(const StringName &p_class) {
    return global_classes.contains(p_class);
}
StringName ScriptServer::get_global_class_language(const StringName &p_class) {
    ERR_FAIL_COND_V(!global_classes.contains(p_class), StringName());
    return global_classes[p_class].language;
}
StringView ScriptServer::get_global_class_path(const StringName & p_class) {
    ERR_FAIL_COND_V(!global_classes.contains(p_class), StringView());
    return global_classes[p_class].path;
}

StringName ScriptServer::get_global_class_base(StringView p_class) {
    ERR_FAIL_COND_V(!global_classes.contains(StringName(p_class)), StringName());
    return global_classes[StringName(p_class)].base;
}
StringName ScriptServer::get_global_class_native_base(const StringName &p_class) {
    ERR_FAIL_COND_V(!global_classes.contains((p_class)), StringName());
    StringName base = global_classes[(p_class)].base;
    while (global_classes.contains(base)) {
        base = global_classes[base].base;
    }
    return base;
}
void ScriptServer::get_global_class_list(Vector<StringName> *r_global_classes) {

    Vector<StringName> classes;

    global_classes.keys_into(classes);

    eastl::stable_sort(classes.begin(),classes.end(),StringName::AlphCompare);
    for (const StringName &e : classes) {
        r_global_classes->emplace_back(e);
    }
}
void ScriptServer::save_global_classes() {
    Vector<StringName> gc;
    get_global_class_list(&gc);
    Array gcarr;
    for (size_t i=0,fin=gc.size(); i<fin; ++i) {
        Dictionary d;
        d["class"] = gc[i];
        d["language"] = global_classes[gc[i]].language;
        d["path"] = global_classes[gc[i]].path;
        d["base"] = global_classes[gc[i]].base;
        gcarr.push_back(d);
    }

    ProjectSettings::get_singleton()->set(StaticCString("_global_script_classes"), gcarr);
    ProjectSettings::get_singleton()->save();
}

////////////////////
void ScriptInstance::get_property_state(Vector<Pair<StringName, Variant>> &state) {

    Vector<PropertyInfo> pinfo;
    get_property_list(&pinfo);

    for(PropertyInfo &E : pinfo ) {
        if (E.usage & PROPERTY_USAGE_STORAGE) {
            Pair<StringName, Variant> p;
            p.first = E.name;
            if (get(p.first, p.second))
                state.push_back(p);
        }
    }
}

Variant ScriptInstance::call(const StringName &p_method, VARIANT_ARG_DECLARE) {

    VARIANT_ARGPTRS
    int argc = 0;
    for (const Variant * i : argptr) {
        if (i->get_type() == VariantType::NIL)
            break;
        argc++;
    }

    Variant::CallError error;
    return call(p_method, argptr, argc, error);
}

void ScriptInstance::call_multilevel(const StringName &p_method, const Variant **p_args, int p_argcount) {
    Variant::CallError ce;
    call(p_method, p_args, p_argcount, ce); // script may not support multilevel calls
}

void ScriptInstance::call_multilevel_reversed(const StringName &p_method, const Variant **p_args, int p_argcount) {
    Variant::CallError ce;
    call(p_method, p_args, p_argcount, ce); // script may not support multilevel calls
}

void ScriptInstance::property_set_fallback(const StringName &, const Variant &, bool *r_valid) {
    if (r_valid)
        *r_valid = false;
}

Variant ScriptInstance::property_get_fallback(const StringName &, bool *r_valid) {
    if (r_valid)
        *r_valid = false;
    return Variant();
}

void ScriptInstance::call_multilevel(const StringName &p_method, VARIANT_ARG_DECLARE) {

    VARIANT_ARGPTRS
    int argc = 0;
    for (const Variant *i : argptr) {
        if (i->get_type() == VariantType::NIL)
            break;
        argc++;
    }

    call_multilevel(p_method, argptr, argc);
}

ScriptCodeCompletionCache *ScriptCodeCompletionCache::singleton = nullptr;
ScriptCodeCompletionCache::ScriptCodeCompletionCache() {
    singleton = this;
}

void ScriptLanguage::frame() {
}

ScriptDebugger *ScriptDebugger::singleton = nullptr;

void ScriptDebugger::set_lines_left(int p_left) {

    lines_left = p_left;
}

int ScriptDebugger::get_lines_left() const {

    return lines_left;
}

void ScriptDebugger::set_depth(int p_depth) {

    depth = p_depth;
}

int ScriptDebugger::get_depth() const {

    return depth;
}

void ScriptDebugger::insert_breakpoint(int p_line, const StringName &p_source) {

    if (!breakpoints.contains(p_line))
        breakpoints[p_line] = HashSet<StringName>();
    breakpoints[p_line].insert(p_source);
}

void ScriptDebugger::remove_breakpoint(int p_line, const StringName &p_source) {

    if (!breakpoints.contains(p_line))
        return;

    breakpoints[p_line].erase(p_source);
    if (breakpoints[p_line].empty())
        breakpoints.erase(p_line);
}
bool ScriptDebugger::is_breakpoint(int p_line, const StringName &p_source) const {

    if (!breakpoints.contains(p_line))
        return false;
    return breakpoints.at(p_line).contains(p_source);
}
bool ScriptDebugger::is_breakpoint_line(int p_line) const {

    return breakpoints.contains(p_line);
}

String ScriptDebugger::breakpoint_find_source(StringView p_source) const {

    return String(p_source);
}

void ScriptDebugger::clear_breakpoints() {

    breakpoints.clear();
}

void ScriptDebugger::idle_poll() {
}

void ScriptDebugger::line_poll() {
}

void ScriptDebugger::set_break_language(ScriptLanguage *p_lang) {

    break_lang = p_lang;
}

ScriptLanguage *ScriptDebugger::get_break_language() const {

    return break_lang;
}

ScriptDebugger::ScriptDebugger() {

    singleton = this;
    lines_left = -1;
    depth = -1;
    break_lang = nullptr;
}

bool PlaceHolderScriptInstance::set(const StringName &p_name, const Variant &p_value) {

    if (script->is_placeholder_fallback_enabled())
        return false;

    if (values.contains(p_name)) {
        Variant defval;
        if (script->get_property_default_value(p_name, defval)) {
            if (defval == p_value) {
                values.erase(p_name);
                return true;
            }
        }
        values[p_name] = p_value;
        return true;
    } else {
        Variant defval;
        if (script->get_property_default_value(p_name, defval)) {
            if (defval != p_value) {
                values[p_name] = p_value;
            }
            return true;
        }
    }
    return false;
}
bool PlaceHolderScriptInstance::get(const StringName &p_name, Variant &r_ret) const {

    if (values.contains(p_name)) {
        r_ret = values.at(p_name);
        return true;
    }

    if (constants.contains(p_name)) {
        r_ret = constants.at(p_name);
        return true;
    }

    if (!script->is_placeholder_fallback_enabled()) {
        Variant defval;
        if (script->get_property_default_value(p_name, defval)) {
            r_ret = defval;
            return true;
        }
    }

    return false;
}

void PlaceHolderScriptInstance::get_property_list(Vector<PropertyInfo> *p_properties) const {

    if (script->is_placeholder_fallback_enabled()) {
        for (const PropertyInfo &E : properties) {
            p_properties->push_back(E);
        }
    } else {
        for (const PropertyInfo &E : properties) {
            PropertyInfo pinfo = E;
            if (!values.contains(pinfo.name)) {
                pinfo.usage |= PROPERTY_USAGE_SCRIPT_DEFAULT_VALUE;
            }
            p_properties->push_back(E);
        }
    }
}

VariantType PlaceHolderScriptInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {

    if (values.contains(p_name)) {
        if (r_is_valid)
            *r_is_valid = true;
        return values.at(p_name).get_type();
    }

    if (constants.contains(p_name)) {
        if (r_is_valid)
            *r_is_valid = true;
        return constants.at(p_name).get_type();
    }

    if (r_is_valid)
        *r_is_valid = false;

    return VariantType::NIL;
}

void PlaceHolderScriptInstance::get_method_list(Vector<MethodInfo> *p_list) const {

    if (script->is_placeholder_fallback_enabled())
        return;

    if (script) {
        script->get_script_method_list(p_list);
    }
}
bool PlaceHolderScriptInstance::has_method(const StringName &p_method) const {

    if (script->is_placeholder_fallback_enabled())
        return false;

    if (script) {
        return script->has_method(p_method);
    }
    return false;
}

void PlaceHolderScriptInstance::update(const Vector<PropertyInfo> &p_properties, const HashMap<StringName, Variant> &p_values) {

    HashSet<StringName> new_values;
    for(const PropertyInfo &E : p_properties ) {

        StringName n = E.name;
        new_values.insert(n);

        if (!values.contains(n) || values[n].get_type() != E.type) {

            if (p_values.contains(n))
                values[n] = p_values.at(n);
        }
    }

    properties = p_properties;
    List<StringName> to_remove;

    for (eastl::pair<const StringName,Variant> &E : values) {

        if (!new_values.contains(E.first))
            to_remove.push_back(E.first);

        Variant defval;
        if (script->get_property_default_value(E.first, defval)) {
            //remove because it's the same as the default value
            if (defval == E.second) {
                to_remove.push_back(E.first);
            }
        }
    }

    while (!to_remove.empty()) {

        values.erase(to_remove.front());
        to_remove.pop_front();
    }

    if (owner && owner->get_script_instance() == this) {

        Object_change_notify(owner);
    }
    //change notify

    constants.clear();
    script->get_constants(&constants);
}

void PlaceHolderScriptInstance::property_set_fallback(const StringName &p_name, const Variant &p_value, bool *r_valid) {

    if (script->is_placeholder_fallback_enabled()) {
        values[p_name] = p_value;

        bool found = false;
        for(const PropertyInfo &F : properties ) {
            if (F.name == p_name) {
                found = true;
                break;
            }
        }
        if (!found) {
            properties.push_back(PropertyInfo(p_value.get_type(), p_name, PropertyHint::None,
                    nullptr, PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_SCRIPT_VARIABLE));
        }
    }

    if (r_valid)
        *r_valid = false; // Cannot change the value in either case
}

Variant PlaceHolderScriptInstance::property_get_fallback(const StringName &p_name, bool *r_valid) {

    if (script->is_placeholder_fallback_enabled()) {
        HashMap<StringName, Variant>::iterator E = values.find(p_name);

        if (E!=values.end()) {
            if (r_valid)
                *r_valid = true;
            return E->second;
        }

        E = constants.find(p_name);
        if (E!=constants.end()) {
            if (r_valid)
                *r_valid = true;
            return E->second;
        }
    }

    if (r_valid)
        *r_valid = false;

    return Variant();
}



PlaceHolderScriptInstance::~PlaceHolderScriptInstance() {

    if (script) {
        script->_placeholder_erased(this);
    }
}
