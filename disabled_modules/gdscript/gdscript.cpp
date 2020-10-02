/*************************************************************************/
/*  gdscript.cpp                                                         */
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

#include "gdscript.h"

#include "gdscript_compiler.h"

#include "core/class_db.h"
#include "core/core_string_names.h"
#include "core/engine.h"
#include "core/global_constants.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/resource_loader.h"
#include "core/method_bind.h"
#include "core/object_db.h"
#include "core/os/file_access.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/pool_vector.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/resource/resource_manager.h"

#include "EASTL/deque.h"
#include "EASTL/sort.h"
using namespace eastl;

IMPL_GDCLASS(GDScriptNativeClass)
IMPL_GDCLASS(GDScript)

///////////////////////////

GDScriptNativeClass::GDScriptNativeClass(const StringName &p_name) {

    name = p_name;
}

bool GDScriptNativeClass::_get(const StringName &p_name, Variant &r_ret) const {

    bool ok;
    int v = ClassDB::get_integer_constant(name, p_name, &ok);

    if (ok) {
        r_ret = v;
        return true;
    } else {
        return false;
    }
}

void GDScriptNativeClass::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("new"), &GDScriptNativeClass::_new);
}

Variant GDScriptNativeClass::_new() {

    Object *o = instance();
    ERR_FAIL_COND_V_MSG(!o, Variant(), "Class type: '" + String(name) + "' is not instantiable.");

    RefCounted *ref = object_cast<RefCounted>(o);
    if (ref) {
        return REF(ref);
    } else {
        return Variant(o);
    }
}

Object *GDScriptNativeClass::instance() {

    return ClassDB::instance(name);
}

GDScriptInstance *GDScript::_create_instance(const Variant **p_args, int p_argcount, Object *p_owner, bool p_isref, Callable::CallError &r_error) {

    /* STEP 1, CREATE */

    GDScriptInstance *instance = memnew(GDScriptInstance);
    instance->base_ref = p_isref;
    instance->members.resize(member_indices.size());
    instance->script = Ref<GDScript>(this);
    instance->owner = p_owner;
#ifdef DEBUG_ENABLED
    //needed for hot reloading
    for (eastl::pair<const StringName,MemberInfo> &E : member_indices) {
        instance->member_indices_cache[E.first] = E.second.index;
    }
#endif
    instance->owner->set_script_instance(instance);

    /* STEP 2, INITIALIZE AND CONSTRUCT */

    GDScriptLanguage::singleton->lock->lock();

    instances.insert(instance->owner);

    GDScriptLanguage::singleton->lock->unlock();

    initializer->call(instance, p_args, p_argcount, r_error);

    if (r_error.error != Callable::CallError::CALL_OK) {
        instance->script = Ref<GDScript>();
        instance->owner->set_script_instance(nullptr);

        GDScriptLanguage::singleton->lock->lock();
        instances.erase(p_owner);
        GDScriptLanguage::singleton->lock->unlock();

        ERR_FAIL_COND_V(r_error.error != Callable::CallError::CALL_OK, nullptr); // error constructing.
    }

    //@TODO make thread safe
    return instance;
}

Variant GDScript::_new(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    /* STEP 1, CREATE */

    if (!valid) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
        return Variant();
    }

    r_error.error = Callable::CallError::CALL_OK;
    REF ref;
    Object *owner = nullptr;

    GDScript *_baseptr = this;
    while (_baseptr->_base) {
        _baseptr = _baseptr->_base;
    }

    ERR_FAIL_COND_V(not _baseptr->native, Variant());
    if (_baseptr->native.get()) {
        owner = _baseptr->native->instance();
    } else {
        owner = memnew(RefCounted); //by default, no base means use reference
    }
    ERR_FAIL_COND_V_MSG(!owner, Variant(), "Can't inherit from a virtual class.");

    RefCounted *r = object_cast<RefCounted>(owner);
    if (r) {
        ref = REF(r);
    }

    GDScriptInstance *instance = _create_instance(p_args, p_argcount, owner, r != nullptr, r_error);
    if (!instance) {
        if (not ref) {
            memdelete(owner); //no owner, sorry
        }
        return Variant();
    }

    if (ref) {
        return ref;
    } else {
        return Variant(owner);
    }
}

bool GDScript::can_instance() const {

#ifdef TOOLS_ENABLED
    return valid && (tool || ScriptServer::is_scripting_enabled());
#else
    return valid;
#endif
}

Ref<Script> GDScript::get_base_script() const {

    if (_base) {
        return Ref<GDScript>(_base);
    } else {
        return Ref<Script>();
    }
}

StringName GDScript::get_instance_base_type() const {

    if (native)
        return native->get_name();
    if (base && base->is_valid())
        return base->get_instance_base_type();
    return StringName();
}

struct _GDScriptMemberSort {

    int index;
    StringName name;
    _FORCE_INLINE_ bool operator<(const _GDScriptMemberSort &p_member) const { return index < p_member.index; }
};

#ifdef TOOLS_ENABLED

void GDScript::_placeholder_erased(PlaceHolderScriptInstance *p_placeholder) {

    placeholders.erase(p_placeholder);
}
#endif

void GDScript::get_script_method_list(Vector<MethodInfo> *p_list) const {

    const GDScript *current = this;
    while (current) {
        for (const eastl::pair<const StringName,GDScriptFunction *> &E : current->member_functions) {
            GDScriptFunction *func = E.second;
            MethodInfo mi;
            mi.name = E.first;
            for (int i = 0; i < func->get_argument_count(); i++) {
                mi.arguments.push_back(func->get_argument_type(i));
            }

            mi.return_val = func->get_return_type();
            p_list->push_back(mi);
        }

        current = current->_base;
    }
}

void GDScript::get_script_property_list(Vector<PropertyInfo> *p_list) const {

    const GDScript *sptr = this;
    Dequeue<PropertyInfo> props;

    while (sptr) {

        Vector<_GDScriptMemberSort> msort;
        for (const eastl::pair<const StringName,PropertyInfo> &E : sptr->member_info) {

            _GDScriptMemberSort ms;
            ERR_CONTINUE(!sptr->member_indices.contains(E.first));
            ms.index = sptr->member_indices.at(E.first).index;
            ms.name = E.first;
            msort.push_back(ms);
        }
        //TODO: SEGS: code below is inefficient, it should sort in reverse instead of sorting+reversing
        eastl::sort(msort.begin(), msort.end());
        eastl::reverse(msort.begin(), msort.end());
        for (auto & i : msort) {

            props.push_front(sptr->member_info.at(i.name));
        }

        sptr = sptr->_base;
    }
    p_list->insert(p_list->end(),props.begin(),props.end());
}

bool GDScript::has_method(const StringName &p_method) const {

    return member_functions.contains(p_method);
}

MethodInfo GDScript::get_method_info(const StringName &p_method) const {

    const Map<StringName, GDScriptFunction *>::const_iterator E = member_functions.find(p_method);
    if (E==member_functions.end())
        return MethodInfo();

    GDScriptFunction *func = E->second;
    MethodInfo mi;
    mi.name = E->first;
    for (int i = 0; i < func->get_argument_count(); i++) {
        mi.arguments.push_back(func->get_argument_type(i));
    }

    mi.return_val = func->get_return_type();
    return mi;
}

bool GDScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {

#ifdef TOOLS_ENABLED

    const HashMap<StringName, Variant>::const_iterator E = member_default_values_cache.find(p_property);
    if (E!=member_default_values_cache.end()) {
        r_value = E->second;
        return true;
    }

    if (base_cache && base_cache->is_valid()) {
        return base_cache->get_property_default_value(p_property, r_value);
    }
#endif
    return false;
}

ScriptInstance *GDScript::instance_create(Object *p_this) {

    GDScript *top = this;
    while (top->_base)
        top = top->_base;

    if (top->native) {
        if (!ClassDB::is_parent_class(p_this->get_class_name(), top->native->get_name())) {

            if (ScriptDebugger::get_singleton()) {
                GDScriptLanguage::get_singleton()->debug_break_parse(get_path(), 1, "Script inherits from native type '" + String(top->native->get_name()) + "', so it can't be instanced in object of type: '" + p_this->get_class() + "'");
            }
            ERR_FAIL_V_MSG(nullptr, "Script inherits from native type '" + String(top->native->get_name()) + "', so it can't be instanced in object of type '" + p_this->get_class() + "'" + ".");
        }
    }

    Callable::CallError unchecked_error;
    return _create_instance(nullptr, 0, p_this, object_cast<RefCounted>(p_this) != nullptr, unchecked_error);
}

PlaceHolderScriptInstance *GDScript::placeholder_instance_create(Object *p_this) {
#ifdef TOOLS_ENABLED
    PlaceHolderScriptInstance *si = memnew(PlaceHolderScriptInstance(GDScriptLanguage::get_singleton(), Ref<Script>(this), p_this));
    placeholders.insert(si);
    _update_exports();
    return si;
#else
    return nullptr;
#endif
}

bool GDScript::instance_has(const Object *p_this) const {

    GDScriptLanguage::singleton->lock->lock();
    bool hasit = instances.contains((Object *)p_this);

    GDScriptLanguage::singleton->lock->unlock();

    return hasit;
}

bool GDScript::has_source_code() const {

    return !source.empty();
}
StringView GDScript::get_source_code() const {

    return source;
}
void GDScript::set_source_code(String p_code) {

    if (source == p_code)
        return;
    source = eastl::move(p_code);
#ifdef TOOLS_ENABLED
    source_changed_cache = true;
#endif
}

#ifdef TOOLS_ENABLED
void GDScript::_update_exports_values(HashMap<StringName, Variant> &values, Vector<PropertyInfo> &propnames) {

    if (base_cache) {
        base_cache->_update_exports_values(values, propnames);
    }

    for (eastl::pair<const StringName,Variant> &E : member_default_values_cache) {
        values[E.first] = E.second;
    }
    propnames.push_back(members_cache);
}
#endif

bool GDScript::_update_exports(bool* r_err = nullptr, bool p_recursive_call = false) {

#ifdef TOOLS_ENABLED

    bool changed = false;

    if (source_changed_cache) {
        source_changed_cache = false;
        changed = true;

        String basedir = path;

        if (basedir.empty())
            basedir = get_path();

        if (!basedir.empty())
            basedir = PathUtils::get_base_dir(basedir);

        GDScriptParser parser;
        Error err = parser.parse(source, basedir, true, path);

        if (err == OK) {

            const GDScriptParser::Node *root = parser.get_parse_tree();
            ERR_FAIL_COND_V(root->type != GDScriptParser::Node::TYPE_CLASS, false);

            const GDScriptParser::ClassNode *c = static_cast<const GDScriptParser::ClassNode *>(root);

            if (base_cache) {
                base_cache->inheriters_cache.erase(get_instance_id());
                base_cache = Ref<GDScript>();
            }

            if (c->extends_used) {
                String path;
                if (!c->extends_file.empty() && c->extends_file.asCString() != get_path()) {
                    path = c->extends_file.asCString();
                    if (PathUtils::is_rel_path(path)) {

                        String base = get_path();
                        if (base.empty() || PathUtils::is_rel_path(base)) {

                            ERR_PRINT("Could not resolve relative path for parent class: " + path);
                        } else {
                            path = PathUtils::plus_file(PathUtils::get_base_dir(base),path);
                        }
                    }
                } else if (!c->extends_class.empty()) {
                    StringName base = c->extends_class[0];

                    if (ScriptServer::is_global_class(base))
                        path = ScriptServer::get_global_class_path(base);
                }

                if (!path.empty()) {
                    if (path != get_path()) {

                        Ref<GDScript> bf = dynamic_ref_cast<GDScript>(gResourceManager().load(path));

                        if (bf) {

                            base_cache = bf;
                            bf->inheriters_cache.insert(get_instance_id());
                        }
                    } else {
                        ERR_PRINT("Path extending itself in  " + path);
                    }
                }
            }

            members_cache.clear();
            member_default_values_cache.clear();

            for (int i = 0; i < c->variables.size(); i++) {
                if (c->variables[i]._export.type == VariantType::NIL)
                    continue;

                members_cache.push_back(c->variables[i]._export);
                member_default_values_cache[c->variables[i].identifier] = c->variables[i].default_value;
            }

            _signals.clear();

            for (int i = 0; i < c->_signals.size(); i++) {
                _signals[c->_signals[i].name] = c->_signals[i].arguments;
            }
        } else {
            placeholder_fallback_enabled = true;
            return false;
        }
    } else if (placeholder_fallback_enabled) {
        return false;
    }

    placeholder_fallback_enabled = false;

    if (base_cache) {
        if (base_cache->_update_exports()) {
            changed = true;
        }
    }

    if (!placeholders.empty()) { //hm :(

        // update placeholders if any
        HashMap<StringName, Variant> values;
        Vector<PropertyInfo> propnames;
        _update_exports_values(values, propnames);

        for (PlaceHolderScriptInstance *E : placeholders) {
            E->update(propnames, values);
        }
    }

    return changed;

#else
    return false;
#endif
}

void GDScript::update_exports() {

#ifdef TOOLS_ENABLED

    _update_exports();

    HashSet<ObjectID> copy = inheriters_cache; //might get modified

    for (ObjectID E : copy) {
        Object *id = gObjectDB().get_instance(E);
        GDScript *s = object_cast<GDScript>(id);
        if (!s)
            continue;
        s->update_exports();
    }

#endif
}

void GDScript::_set_subclass_path(Ref<GDScript> &p_sc, StringView p_path) {

    p_sc->path = p_path;
    for (eastl::pair<const StringName,Ref<GDScript> > &E : p_sc->subclasses) {

        _set_subclass_path(E.second, p_path);
    }
}

Error GDScript::reload(bool p_keep_state) {

    GDScriptLanguage::singleton->lock->lock();
    bool has_instances = !instances.empty();

    GDScriptLanguage::singleton->lock->unlock();

    ERR_FAIL_COND_V(!p_keep_state && has_instances, ERR_ALREADY_IN_USE);

    String basedir = path;

    if (basedir.empty())
        basedir = get_path();

    if (!basedir.empty())
        basedir = PathUtils::get_base_dir(basedir);

    if (StringUtils::contains(source,"%BASE%") ) {
        //loading a template, don't parse
        return OK;
    }

    valid = false;
    GDScriptParser parser;
    Error err = parser.parse(source, basedir, false, path);
    if (err) {
        if (ScriptDebugger::get_singleton()) {
            GDScriptLanguage::get_singleton()->debug_break_parse(get_path(), parser.get_error_line(), "Parser Error: " + parser.get_error());
        }
        _err_print_error("GDScript::reload", path.empty() ? "built-in" : path.c_str(),
                         parser.get_error_line(), ("Parse Error: " + parser.get_error()), {},ERR_HANDLER_SCRIPT);
        ERR_FAIL_V(ERR_PARSE_ERROR);
    }

    bool can_run = ScriptServer::is_scripting_enabled() || parser.is_tool_script();

    GDScriptCompiler compiler;
    err = compiler.compile(&parser, this, p_keep_state);

    if (err) {

        if (can_run) {
            if (ScriptDebugger::get_singleton()) {
                GDScriptLanguage::get_singleton()->debug_break_parse(get_path(), compiler.get_error_line(), "Parser Error: " + compiler.get_error());
            }
            _err_print_error("GDScript::reload", path.empty() ? "built-in" : path.c_str(), compiler.get_error_line(),
                             ("Compile Error: " + compiler.get_error()), {},ERR_HANDLER_SCRIPT);
            ERR_FAIL_V(ERR_COMPILATION_FAILED);
        } else {
            return err;
        }
    }
#ifdef DEBUG_ENABLED
    Vector<ScriptLanguage::StackInfo> si;
    for (const GDScriptWarning &warning : parser.get_warnings()) {
        if (ScriptDebugger::get_singleton()) {
            ScriptDebugger::get_singleton()->send_error({}, get_path(), warning.line, warning.get_name(), warning.get_message(), ERR_HANDLER_WARNING, si);
        }
    }
#endif

    valid = true;

    for (eastl::pair<const StringName,Ref<GDScript> > &E : subclasses) {

        _set_subclass_path(E.second, path);
    }

    return OK;
}

ScriptLanguage *GDScript::get_language() const {

    return GDScriptLanguage::get_singleton();
}

void GDScript::get_constants(HashMap<StringName, Variant> *p_constants) {

    if (p_constants) {
        for (eastl::pair<const StringName,Variant> &E : constants) {
            (*p_constants)[E.first] = E.second;
        }
    }
}

void GDScript::get_members(HashSet<StringName> *p_members) {
    if (p_members) {
        for (const StringName &E : members) {
            p_members->insert(E);
        }
    }
}

Variant GDScript::call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    GDScript *top = this;
    while (top) {

        Map<StringName, GDScriptFunction *>::iterator E = top->member_functions.find(p_method);
        if (E!=top->member_functions.end()) {

            ERR_FAIL_COND_V_MSG(!E->second->is_static(), Variant(), "Can't call non-static function '" + String(p_method) + "' in script.");

            return E->second->call(nullptr, p_args, p_argcount, r_error);
        }
        top = top->_base;
    }

    //none found, regular

    return Script::call(p_method, p_args, p_argcount, r_error);
}

bool GDScript::_get(const StringName &p_name, Variant &r_ret) const {

    {

        const GDScript *top = this;
        while (top) {

            {
                const HashMap<StringName, Variant>::const_iterator E = top->constants.find(p_name);
                if (E!=top->constants.end()) {

                    r_ret = E->second;
                    return true;
                }
            }

            {
                const Map<StringName, Ref<GDScript> >::const_iterator E = subclasses.find(p_name);
                if (E!=subclasses.end()) {

                    r_ret = E->second;
                    return true;
                }
            }
            top = top->_base;
        }

        if (p_name == GDScriptLanguage::get_singleton()->strings._script_source) {

            r_ret = get_source_code();
            return true;
        }
    }

    return false;
}
bool GDScript::_set(const StringName &p_name, const Variant &p_value) {

    if (p_name == GDScriptLanguage::get_singleton()->strings._script_source) {

        set_source_code(p_value.as<String>());
        reload();
    } else
        return false;

    return true;
}

void GDScript::_get_property_list(Vector<PropertyInfo> *p_properties) const {

    p_properties->emplace_back(PropertyInfo(VariantType::STRING, "script/source", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
}

void GDScript::_bind_methods() {

    MethodBinder::bind_vararg_method("new", &GDScript::_new, MethodInfo("new"));

    MethodBinder::bind_method(D_METHOD("get_as_byte_code"), &GDScript::get_as_byte_code);
}

Vector<uint8_t> GDScript::get_as_byte_code() const {

    return GDScriptTokenizerBuffer::parse_code_string(source);
};

Error GDScript::load_byte_code(StringView p_path) {

    Vector<uint8_t> bytecode;

    if (StringUtils::ends_with(p_path,"gde")) {

        FileAccess *fa = FileAccess::open(p_path, FileAccess::READ);
        ERR_FAIL_COND_V(!fa, ERR_CANT_OPEN);

        FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
        ERR_FAIL_COND_V(!fae, ERR_CANT_OPEN);

        uint8_t key[32];
        for (int i = 0; i < 32; i++) {
            key[i] = script_encryption_key[i];
        }

        Error err = fae->open_and_parse(fa, key, FileAccessEncrypted::MODE_READ);

        if (err) {
            fa->close();
            memdelete(fa);
            memdelete(fae);

            ERR_FAIL_COND_V(err, err);
        }

        bytecode.resize(fae->get_len());
        fae->get_buffer(bytecode.data(), bytecode.size());
        fae->close();
        memdelete(fae);

    } else {

        bytecode = FileAccess::get_file_as_array(p_path);
    }

    ERR_FAIL_COND_V(bytecode.empty(), ERR_PARSE_ERROR);
    path = p_path;

    String basedir = path;

    if (basedir.empty())
        basedir = get_path();

    if (!basedir.empty())
        basedir = PathUtils::get_base_dir(basedir);

    valid = false;
    GDScriptParser parser;
    Error err = parser.parse_bytecode(bytecode, basedir, get_path());
    if (err) {
        _err_print_error("GDScript::load_byte_code", path.empty() ? "built-in" : path.c_str(), parser.get_error_line(),
                ("Parse Error: " + parser.get_error()), {},ERR_HANDLER_SCRIPT);
        ERR_FAIL_V(ERR_PARSE_ERROR);
    }

    GDScriptCompiler compiler;
    err = compiler.compile(&parser, this);

    if (err) {
        _err_print_error("GDScript::load_byte_code", path.empty() ? "built-in" : path.c_str(),
                compiler.get_error_line(), ("Compile Error: " + compiler.get_error()), {},ERR_HANDLER_SCRIPT);
        ERR_FAIL_V(ERR_COMPILATION_FAILED);
    }

    valid = true;

    for (eastl::pair<const StringName,Ref<GDScript> > &E : subclasses) {

        _set_subclass_path(E.second, path);
    }

    return OK;
}

Error GDScript::load_source_code(StringView p_path) {

    Vector<uint8_t> sourcef;
    Error err;
    FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);
    if (err) {

        ERR_FAIL_COND_V(err, err);
    }

    int len = f->get_len();
    sourcef.resize(len + 1);
    int r = f->get_buffer(sourcef.data(), len);
    f->close();
    memdelete(f);
    ERR_FAIL_COND_V(r != len, ERR_CANT_OPEN);
    sourcef[len] = 0;

    StringView s((const char *)sourcef.data(),len+1);
    if (s.empty()) {

        ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Script '" + String(p_path) + "' contains invalid unicode (UTF-8), so it was not loaded. Please ensure that scripts are saved in valid UTF-8 unicode.");
    }

    source = s;
#ifdef TOOLS_ENABLED
    source_changed_cache = true;
#endif
    path = p_path;
    return OK;
}

const Map<StringName, GDScriptFunction *> &GDScript::debug_get_member_functions() const {

    return member_functions;
}

StringName GDScript::debug_get_member_by_index(int p_idx) const {

    for (const eastl::pair<const StringName,MemberInfo> &E : member_indices) {

        if (E.second.index == p_idx)
            return E.first;
    }

    return "<error>";
}

Ref<GDScript> GDScript::get_base() const {

    return base;
}

bool GDScript::has_script_signal(const StringName &p_signal) const {
    if (_signals.contains(p_signal))
        return true;
    if (base) {
        return base->has_script_signal(p_signal);
    }
#ifdef TOOLS_ENABLED
    else if (base_cache) {
        return base_cache->has_script_signal(p_signal);
    }
#endif
    return false;
}
void GDScript::get_script_signal_list(Vector<MethodInfo> *r_signals) const {

    for (const eastl::pair<const StringName, Vector<StringName> > &E : _signals) {

        MethodInfo mi;
        mi.name = E.first;
        for (int i = 0; i < E.second.size(); i++) {
            PropertyInfo arg;
            arg.name = E.second[i];
            mi.arguments.push_back(arg);
        }
        r_signals->push_back(mi);
    }

    if (base) {
        base->get_script_signal_list(r_signals);
    }
#ifdef TOOLS_ENABLED
    else if (base_cache) {
        base_cache->get_script_signal_list(r_signals);
    }

#endif
}

GDScript::GDScript() :
        script_list(this) {
    //BUG: this inherits from Reference but Variant is constructed as for raw Object *
    _static_ref = Variant(this);
    valid = false;
    subclass_count = 0;
    initializer = nullptr;
    _base = nullptr;
    _owner = nullptr;
    tool = false;
#ifdef TOOLS_ENABLED
    source_changed_cache = false;
    placeholder_fallback_enabled = false;
#endif

#ifdef DEBUG_ENABLED
    if (GDScriptLanguage::get_singleton()->lock) {
        GDScriptLanguage::get_singleton()->lock->lock();
    }
    GDScriptLanguage::get_singleton()->script_list.add(&script_list);

    if (GDScriptLanguage::get_singleton()->lock) {
        GDScriptLanguage::get_singleton()->lock->unlock();
    }
#endif
}
void GDScript::_save_orphaned_subclasses() {
    struct ClassRefWithName {
        ObjectID id;
        String fully_qualified_name;
    };
    Vector<ClassRefWithName> weak_subclasses;
    // collect subclasses ObjectID and name
    for (const auto &E : subclasses) {
        E.second->_owner = nullptr; //bye, you are no longer owned cause I died
        ClassRefWithName subclass;
        subclass.id = E.second->get_instance_id();
        subclass.fully_qualified_name = E.second->fully_qualified_name;
        weak_subclasses.push_back(subclass);
    }

    // clear subclasses to allow unused subclasses to be deleted
    subclasses.clear();
    // subclasses are also held by constants, clear those as well
    constants.clear();

    // keep orphan subclass only for subclasses that are still in use
    for (int i = 0; i < weak_subclasses.size(); i++) {
        ClassRefWithName subclass = weak_subclasses[i];
        Object *obj = gObjectDB().get_instance(subclass.id);
        if (!obj)
            continue;
        // subclass is not released
        GDScriptLanguage::get_singleton()->add_orphan_subclass(subclass.fully_qualified_name, subclass.id);
    }
}
GDScript::~GDScript() {
    for (eastl::pair<const StringName,GDScriptFunction *> &E : member_functions) {
        memdelete(E.second);
    }

    _save_orphaned_subclasses();

#ifdef DEBUG_ENABLED
    if (GDScriptLanguage::get_singleton()->lock) {
        GDScriptLanguage::get_singleton()->lock->lock();
    }
    GDScriptLanguage::get_singleton()->script_list.remove(&script_list);

    if (GDScriptLanguage::get_singleton()->lock) {
        GDScriptLanguage::get_singleton()->lock->unlock();
    }
#endif
}

//////////////////////////////
//         INSTANCE         //
//////////////////////////////

bool GDScriptInstance::set(const StringName &p_name, const Variant &p_value) {

    //member
    {
        const Map<StringName, GDScript::MemberInfo>::iterator E = script->member_indices.find(p_name);
        if (E!=script->member_indices.end()) {
            const GDScript::MemberInfo *member = &E->second;
            if (member->setter) {
                const Variant *val = &p_value;
                Callable::CallError err;
                call(member->setter, &val, 1, err);
                if (err.error == Callable::CallError::CALL_OK) {
                    return true; //function exists, call was successful
                }
            } else {
                if (!member->data_type.is_type(p_value)) {
                    // Try conversion
                    Callable::CallError ce;
                    const Variant *value = &p_value;
                    Variant converted = Variant::construct(member->data_type.builtin_type, &value, 1, ce);
                    if (ce.error == Callable::CallError::CALL_OK) {
                        members[member->index] = converted;
                        return true;
                    } else {
                        return false;
                    }
                } else {
                    members[member->index] = p_value;
                }
            }
            return true;
        }
    }

    GDScript *sptr = script.get();
    while (sptr) {

        Map<StringName, GDScriptFunction *>::iterator E = sptr->member_functions.find(GDScriptLanguage::get_singleton()->strings._set);
        if (E!=sptr->member_functions.end()) {

            Variant name = p_name;
            const Variant *args[2] = { &name, &p_value };

            Callable::CallError err;
            Variant ret = E->second->call(this, (const Variant **)args, 2, err);
            if (err.error == Callable::CallError::CALL_OK && ret.get_type() == VariantType::BOOL && ret.operator bool())
                return true;
        }
        sptr = sptr->_base;
    }

    return false;
}

bool GDScriptInstance::get(const StringName &p_name, Variant &r_ret) const {

    const GDScript *sptr = script.get();
    while (sptr) {

        {
            const Map<StringName, GDScript::MemberInfo>::const_iterator E = script->member_indices.find(p_name);
            if (E!=script->member_indices.end()) {
                if (E->second.getter) {
                    Callable::CallError err;
                    r_ret = const_cast<GDScriptInstance *>(this)->call(E->second.getter, nullptr, 0, err);
                    if (err.error == Callable::CallError::CALL_OK) {
                        return true;
                    }
                }
                r_ret = members[E->second.index];
                return true; //index found
            }
        }

        {

            const GDScript *sl = sptr;
            while (sl) {
                const HashMap<StringName, Variant>::const_iterator E = sl->constants.find(p_name);
                if (E!=sl->constants.end()) {
                    r_ret = E->second;
                    return true; //index found
                }
                sl = sl->_base;
            }
        }

        {
            const Map<StringName, GDScriptFunction *>::const_iterator E = sptr->member_functions.find(GDScriptLanguage::get_singleton()->strings._get);
            if (E!=sptr->member_functions.end()) {

                Variant name = p_name;
                const Variant *args[1] = { &name };

                Callable::CallError err;
                Variant ret = const_cast<GDScriptFunction *>(E->second)->call(const_cast<GDScriptInstance *>(this), (const Variant **)args, 1, err);
                if (err.error == Callable::CallError::CALL_OK && ret.get_type() != VariantType::NIL) {
                    r_ret = ret;
                    return true;
                }
            }
        }
        sptr = sptr->_base;
    }

    return false;
}

VariantType GDScriptInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {

    const GDScript *sptr = script.get();
    while (sptr) {

        if (sptr->member_info.contains(p_name)) {
            if (r_is_valid)
                *r_is_valid = true;
            return sptr->member_info.at(p_name).type;
        }
        sptr = sptr->_base;
    }

    if (r_is_valid)
        *r_is_valid = false;
    return VariantType::NIL;
}

void GDScriptInstance::get_property_list(Vector<PropertyInfo> *p_properties) const {
    // exported members, not done yet!

    const GDScript *sptr = script.get();
    Dequeue<PropertyInfo> props;

    while (sptr) {

        const Map<StringName, GDScriptFunction *>::const_iterator E = sptr->member_functions.find(GDScriptLanguage::get_singleton()->strings._get_property_list);
        if (E!=sptr->member_functions.end()) {

            Callable::CallError err;
            Variant ret = const_cast<GDScriptFunction *>(E->second)->call(const_cast<GDScriptInstance *>(this), nullptr, 0, err);
            if (err.error == Callable::CallError::CALL_OK) {

                ERR_FAIL_COND_MSG(ret.get_type() != VariantType::ARRAY, "Wrong type for _get_property_list, must be an array of dictionaries.");

                Array arr = ret;
                for (int i = 0; i < arr.size(); i++) {

                    Dictionary d = arr[i];
                    ERR_CONTINUE(!d.has("name"));
                    ERR_CONTINUE(!d.has("type"));
                    PropertyInfo pinfo;
                    pinfo.type = VariantType(d["type"].operator int());
                    ERR_CONTINUE(int8_t(pinfo.type) < 0 || int8_t(pinfo.type) >= int8_t(VariantType::VARIANT_MAX));
                    pinfo.name = d["name"];
                    ERR_CONTINUE(pinfo.name.empty());
                    if (d.has("hint"))
                        pinfo.hint = PropertyHint(d["hint"].operator int());
                    if (d.has("hint_string"))
                        pinfo.hint_string = d["hint_string"].as<String>();
                    if (d.has("usage"))
                        pinfo.usage = d["usage"];

                    props.emplace_back(pinfo);
                }
            }
        }

        //instance a fake script for editing the values

        Vector<_GDScriptMemberSort> msort;
        for (const eastl::pair<const StringName,PropertyInfo> &F : sptr->member_info) {

            _GDScriptMemberSort ms;
            ERR_CONTINUE(!sptr->member_indices.contains(F.first));
            ms.index = sptr->member_indices.at(F.first).index;
            ms.name = F.first;
            msort.push_back(ms);
        }
        //TODO: SEGS: code below is inefficient, it should sort in reverse instead of sorting+reversing
        eastl::sort(msort.begin(), msort.end());
        eastl::reverse(msort.begin(), msort.end());

        for (int i = 0; i < msort.size(); i++) {

            props.emplace_front(sptr->member_info.at(msort[i].name));
        }

        sptr = sptr->_base;
    }

    for (const PropertyInfo &E : props) {

        p_properties->push_back(E);
    }
}

void GDScriptInstance::get_method_list(Vector<MethodInfo> *p_list) const {

    const GDScript *sptr = script.get();
    while (sptr) {

        for (const eastl::pair<const StringName,GDScriptFunction *> &E : sptr->member_functions) {

            MethodInfo mi;
            mi.name = E.first;
            mi.flags |= METHOD_FLAG_FROM_SCRIPT;
            for (int i = 0; i < E.second->get_argument_count(); i++)
                mi.arguments.push_back(PropertyInfo(VariantType::NIL, StringName("arg" + ::to_string(i))));
            p_list->push_back(mi);
        }
        sptr = sptr->_base;
    }
}

bool GDScriptInstance::has_method(const StringName &p_method) const {

    const GDScript *sptr = script.get();
    while (sptr) {
        if (sptr->member_functions.contains(p_method))
            return true;
        sptr = sptr->_base;
    }

    return false;
}
Variant GDScriptInstance::call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    GDScript *sptr = script.get();
    while (sptr) {
        Map<StringName, GDScriptFunction *>::iterator E = sptr->member_functions.find(p_method);
        if (E!=sptr->member_functions.end()) {
            return E->second->call(this, p_args, p_argcount, r_error);
        }
        sptr = sptr->_base;
    }
    r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
    return Variant();
}

void GDScriptInstance::call_multilevel(const StringName &p_method, const Variant **p_args, int p_argcount) {

    GDScript *sptr = script.get();
    Callable::CallError ce;

    while (sptr) {
        Map<StringName, GDScriptFunction *>::iterator E = sptr->member_functions.find(p_method);
        if (E!=sptr->member_functions.end()) {
            E->second->call(this, p_args, p_argcount, ce);
        }
        sptr = sptr->_base;
    }
}

void GDScriptInstance::_ml_call_reversed(GDScript *sptr, const StringName &p_method, const Variant **p_args, int p_argcount) {

    if (sptr->_base)
        _ml_call_reversed(sptr->_base, p_method, p_args, p_argcount);

    Callable::CallError ce;

    Map<StringName, GDScriptFunction *>::iterator E = sptr->member_functions.find(p_method);
    if (E!=sptr->member_functions.end()) {
        E->second->call(this, p_args, p_argcount, ce);
    }
}

void GDScriptInstance::call_multilevel_reversed(const StringName &p_method, const Variant **p_args, int p_argcount) {

    if (script.get()) {
        _ml_call_reversed(script.get(), p_method, p_args, p_argcount);
    }
}

void GDScriptInstance::notification(int p_notification) {

    //notification is not virtual, it gets called at ALL levels just like in C.
    Variant value = p_notification;
    const Variant *args[1] = { &value };

    GDScript *sptr = script.get();
    while (sptr) {
        Map<StringName, GDScriptFunction *>::iterator E = sptr->member_functions.find(GDScriptLanguage::get_singleton()->strings._notification);
        if (E!=sptr->member_functions.end()) {
            Callable::CallError err;
            E->second->call(this, args, 1, err);
            if (err.error != Callable::CallError::CALL_OK) {
                //print error about notification call
            }
        }
        sptr = sptr->_base;
    }
}

String GDScriptInstance::to_string(bool *r_valid) {
    if (has_method(CoreStringNames::get_singleton()->_to_string)) {
        Callable::CallError ce;
        Variant ret = call(CoreStringNames::get_singleton()->_to_string, nullptr, 0, ce);
        if (ce.error == Callable::CallError::CALL_OK) {
            if (ret.get_type() != VariantType::STRING) {
                if (r_valid)
                    *r_valid = false;
                ERR_FAIL_V_MSG(String(), String("Wrong type for ") + CoreStringNames::get_singleton()->_to_string + ", must be a String.");
            }
            if (r_valid)
                *r_valid = true;
            return ret.as<String>();
        }
    }
    if (r_valid)
        *r_valid = false;
    return String();
}

Ref<Script> GDScriptInstance::get_script() const {

    return script;
}

ScriptLanguage *GDScriptInstance::get_language() {

    return GDScriptLanguage::get_singleton();
}

MultiplayerAPI_RPCMode GDScriptInstance::get_rpc_mode(const StringName &p_method) const {

    const GDScript *cscript = script.get();

    while (cscript) {
        const Map<StringName, GDScriptFunction *>::const_iterator E = cscript->member_functions.find(p_method);
        if (E!=cscript->member_functions.end()) {

            if (E->second->get_rpc_mode() != MultiplayerAPI_RPCMode(0)) {
                return E->second->get_rpc_mode();
            }
        }
        cscript = cscript->_base;
    }

    return MultiplayerAPI_RPCMode(0);
}

MultiplayerAPI_RPCMode GDScriptInstance::get_rset_mode(const StringName &p_variable) const {

    const GDScript *cscript = script.get();

    while (cscript) {
        const Map<StringName, GDScript::MemberInfo>::const_iterator E = cscript->member_indices.find(p_variable);
        if (E!=cscript->member_indices.end()) {

            if (E->second.rpc_mode) {
                return E->second.rpc_mode;
            }
        }
        cscript = cscript->_base;
    }

    return MultiplayerAPI_RPCMode(0);
}

void GDScriptInstance::reload_members() {

#ifdef DEBUG_ENABLED

    members.resize(script->member_indices.size()); //resize

    Vector<Variant> new_members;
    new_members.resize(script->member_indices.size());

    //pass the values to the new indices
    for (eastl::pair<const StringName,GDScript::MemberInfo> &E : script->member_indices) {

        if (member_indices_cache.contains(E.first)) {
            Variant value = members[member_indices_cache[E.first]];
            new_members[E.second.index] = value;
        }
    }

    //apply
    members = new_members;

    //pass the values to the new indices
    member_indices_cache.clear();
    for (eastl::pair<const StringName,GDScript::MemberInfo> &E : script->member_indices) {

        member_indices_cache[E.first] = E.second.index;
    }

#endif
}

GDScriptInstance::GDScriptInstance() {
    owner = nullptr;
    base_ref = false;
}

GDScriptInstance::~GDScriptInstance() {
    if (script && owner) {
        GDScriptLanguage::singleton->lock->lock();

        script->instances.erase(owner);
        GDScriptLanguage::singleton->lock->unlock();
    }
}

/************* SCRIPT LANGUAGE **************/

GDScriptLanguage *GDScriptLanguage::singleton = nullptr;

StringName GDScriptLanguage::get_name() const {

    return StringName("GDScript");
}

/* LANGUAGE FUNCTIONS */

void GDScriptLanguage::_add_global(const StringName &p_name, const Variant &p_value) {

    if (globals.contains(p_name)) {
        //overwrite existing
        global_array[globals[p_name]] = p_value;
        return;
    }
    globals[p_name] = global_array.size();
    global_array.push_back(p_value);
    _global_array = global_array.data();
}

void GDScriptLanguage::add_global_constant(const StringName &p_variable, const Variant &p_value) {

    _add_global(p_variable, p_value);
}

void GDScriptLanguage::add_named_global_constant(const StringName &p_name, const Variant &p_value) {
    named_globals[p_name] = p_value;
}

void GDScriptLanguage::remove_named_global_constant(const StringName &p_name) {
    ERR_FAIL_COND(!named_globals.contains(p_name));
    named_globals.erase(p_name);
}

void GDScriptLanguage::init() {

    //populate global constants
    int gcc = GlobalConstants::get_global_constant_count();
    for (int i = 0; i < gcc; i++) {

        _add_global(StaticCString(GlobalConstants::get_global_constant_name(i),true), GlobalConstants::get_global_constant_value(i));
    }

    _add_global("PI", Math_PI);
    _add_global("TAU", Math_TAU);
    _add_global("INF", Math_INF);
    _add_global("NAN", Math_NAN);

    //populate native classes

    Vector<StringName> class_list;
    ClassDB::get_class_list(&class_list);
    for (size_t i=0,fin=class_list.size(); i<fin; ++i) {

        StringName n = class_list[i];
        StringView s(n);
        if (StringUtils::begins_with(s,"_"))
            n = StringName(StringUtils::substr(s,1));

        if (globals.contains(n))
            continue;
        Ref<GDScriptNativeClass> nc(make_ref_counted<GDScriptNativeClass>(class_list[i]));
        _add_global(n, nc);
    }

    //populate singletons

    auto &singletons(Engine::get_singleton()->get_singletons());
    for (const Engine::Singleton &E : singletons) {

        _add_global(E.name, Variant(E.ptr));
    }
}

String GDScriptLanguage::get_type() const {

    return String("GDScript");
}
String GDScriptLanguage::get_extension() const {

    return String("gd");
}
Error GDScriptLanguage::execute_file(StringView p_path) {

    // ??
    return OK;
}
void GDScriptLanguage::finish() {
}

void GDScriptLanguage::profiling_start() {

#ifdef DEBUG_ENABLED
    if (lock) {
        lock->lock();
    }

    IntrusiveListNode<GDScriptFunction> *elem = function_list.first();
    while (elem) {
        elem->self()->profile.call_count = 0;
        elem->self()->profile.self_time = 0;
        elem->self()->profile.total_time = 0;
        elem->self()->profile.frame_call_count = 0;
        elem->self()->profile.frame_self_time = 0;
        elem->self()->profile.frame_total_time = 0;
        elem->self()->profile.last_frame_call_count = 0;
        elem->self()->profile.last_frame_self_time = 0;
        elem->self()->profile.last_frame_total_time = 0;
        elem = elem->next();
    }

    profiling = true;
    if (lock) {
        lock->unlock();
    }

#endif
}

void GDScriptLanguage::profiling_stop() {

#ifdef DEBUG_ENABLED
    if (lock) {
        lock->lock();
    }

    profiling = false;
    if (lock) {
        lock->unlock();
    }

#endif
}

int GDScriptLanguage::profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) {

    int current = 0;
#ifdef DEBUG_ENABLED
    if (lock) {
        lock->lock();
    }

    IntrusiveListNode<GDScriptFunction> *elem = function_list.first();
    while (elem) {
        if (current >= p_info_max)
            break;
        p_info_arr[current].call_count = elem->self()->profile.call_count;
        p_info_arr[current].self_time = elem->self()->profile.self_time;
        p_info_arr[current].total_time = elem->self()->profile.total_time;
        p_info_arr[current].signature = elem->self()->profile.signature;
        elem = elem->next();
        current++;
    }

    if (lock) {
        lock->unlock();
    }

#endif

    return current;
}

int GDScriptLanguage::profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) {

    int current = 0;

#ifdef DEBUG_ENABLED
    if (lock) {
        lock->lock();
    }

    IntrusiveListNode<GDScriptFunction> *elem = function_list.first();
    while (elem) {
        if (current >= p_info_max)
            break;
        if (elem->self()->profile.last_frame_call_count > 0) {
            p_info_arr[current].call_count = elem->self()->profile.last_frame_call_count;
            p_info_arr[current].self_time = elem->self()->profile.last_frame_self_time;
            p_info_arr[current].total_time = elem->self()->profile.last_frame_total_time;
            p_info_arr[current].signature = elem->self()->profile.signature;
            current++;
        }
        elem = elem->next();
    }

    if (lock) {
        lock->unlock();
    }

#endif

    return current;
}

struct GDScriptDepSort {

    //must support sorting so inheritance works properly (parent must be reloaded first)
    bool operator()(const Ref<GDScript> &A, const Ref<GDScript> &B) const {

        if (A == B)
            return false; //shouldn't happen but..
        const GDScript *I = B->get_base().get();
        while (I) {
            if (I == A.get()) {
                // A is a base of B
                return true;
            }

            I = I->get_base().get();
        }

        return false; //not a base
    }
};

void GDScriptLanguage::reload_all_scripts() {

#ifdef DEBUG_ENABLED
    print_verbose("GDScript: Reloading all scripts");
    if (lock) {
        lock->lock();
    }

    Vector< Ref<GDScript> > scripts;

    IntrusiveListNode<GDScript> *elem = script_list.first();
    while (elem) {
        if (PathUtils::is_resource_file(elem->self()->get_path())) {
            print_verbose("GDScript: Found: " + elem->self()->get_path());
            scripts.push_back(Ref<GDScript>(elem->self())); //cast to gdscript to avoid being erased by accident
        }
        elem = elem->next();
    }

    if (lock) {
        lock->unlock();
    }

    //as scripts are going to be reloaded, must proceed without locking here
    eastl::sort(scripts.begin(),scripts.end(),GDScriptDepSort()); //update in inheritance dependency order

    for (Ref<GDScript> &E : scripts) {

        print_verbose("GDScript: Reloading: " + E->get_path());
        E->load_source_code(E->get_path());
        E->reload(true);
    }
#endif
}

void GDScriptLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {

#ifdef DEBUG_ENABLED

    if (lock) {
        lock->lock();
    }

    Vector<Ref<GDScript> > scripts;

    IntrusiveListNode<GDScript> *elem = script_list.first();
    while (elem) {
        if (PathUtils::is_resource_file(elem->self()->get_path())) {

            scripts.push_back(Ref<GDScript>(elem->self())); //cast to gdscript to avoid being erased by accident
        }
        elem = elem->next();
    }

    if (lock) {
        lock->unlock();
    }

    //when someone asks you why dynamically typed languages are easier to write....

    Map<Ref<GDScript>, Map<ObjectID, Vector<Pair<StringName, Variant> > > > to_reload;

    //as scripts are going to be reloaded, must proceed without locking here

    //update in inheritance dependency order
    eastl::sort(eastl::make_move_iterator(scripts.begin()),eastl::make_move_iterator(scripts.end()),GDScriptDepSort());

    for (const Ref<GDScript> &E : scripts) {

        bool reload = Ref<Script>(E) == p_script || to_reload.contains(E->get_base());

        if (!reload)
            continue;

        to_reload.emplace(E, Map<ObjectID, Vector<Pair<StringName, Variant> > >());

        if (!p_soft_reload) {

            //save state and remove script from instances
            Map<ObjectID, Vector<Pair<StringName, Variant> > > &map = to_reload[E];

            while (E->instances.begin()!=E->instances.end()) {
                Object *obj = *E->instances.begin();
                //save instance info
                Vector<Pair<StringName, Variant> > state;
                if (obj->get_script_instance()) {

                    obj->get_script_instance()->get_property_state(state);
                    map[obj->get_instance_id()] = eastl::move(state);
                    obj->set_script(RefPtr());
                }
            }

//same thing for placeholders
#ifdef TOOLS_ENABLED

            while (!E->placeholders.empty()) {
                Object *obj = (*E->placeholders.begin())->get_owner();

                //save instance info
                if (obj->get_script_instance()) {

                    map.emplace(obj->get_instance_id(), Vector<Pair<StringName, Variant> >());
                    Vector<Pair<StringName, Variant> > &state = map[obj->get_instance_id()];
                    obj->get_script_instance()->get_property_state(state);
                    obj->set_script(RefPtr());
                } else {
                    // no instance found. Let's remove it so we don't loop forever
                    E->placeholders.erase(E->placeholders.begin());
                }
            }

#endif

            for (auto &F : E->pending_reload_state) {
                map[F.first] = F.second; //pending to reload, use this one instead
            }
        }
    }

    for (auto &E : to_reload) {

        Ref<GDScript> scr = E.first;
        scr->reload(p_soft_reload);

        //restore state if saved
        for (auto &F : E.second) {

            Vector<Pair<StringName, Variant> > &saved_state = F.second;

            Object *obj = gObjectDB().get_instance(F.first);
            if (!obj)
                continue;

            if (!p_soft_reload) {
                //clear it just in case (may be a pending reload state)
                obj->set_script(RefPtr());
            }
            obj->set_script(scr.get_ref_ptr());

            ScriptInstance *script_instance = obj->get_script_instance();

            if (!script_instance) {
                //failed, save reload state for next time if not saved
                if (!scr->pending_reload_state.contains(obj->get_instance_id())) {
                    scr->pending_reload_state[obj->get_instance_id()] = saved_state;
                }
                continue;
            }

            if (script_instance->is_placeholder() && scr->is_placeholder_fallback_enabled()) {
                PlaceHolderScriptInstance *placeholder = static_cast<PlaceHolderScriptInstance *>(script_instance);
                for (const Pair<StringName, Variant> &G : saved_state) {
                    placeholder->property_set_fallback(G.first, G.second);
                }
            } else {
                for (const Pair<StringName, Variant> &G : saved_state) {
                    script_instance->set(G.first, G.second);
                }
            }

            scr->pending_reload_state.erase(obj->get_instance_id()); //as it reloaded, remove pending state
        }

        //if instance states were saved, set them!
    }

#endif
}

void GDScriptLanguage::frame() {

    calls = 0;

#ifdef DEBUG_ENABLED
    if (profiling) {
        if (lock) {
            lock->lock();
        }

        IntrusiveListNode<GDScriptFunction> *elem = function_list.first();
        while (elem) {
            elem->self()->profile.last_frame_call_count = elem->self()->profile.frame_call_count;
            elem->self()->profile.last_frame_self_time = elem->self()->profile.frame_self_time;
            elem->self()->profile.last_frame_total_time = elem->self()->profile.frame_total_time;
            elem->self()->profile.frame_call_count = 0;
            elem->self()->profile.frame_self_time = 0;
            elem->self()->profile.frame_total_time = 0;
            elem = elem->next();
        }

        if (lock) {
            lock->unlock();
        }
    }

#endif
}

/* EDITOR FUNCTIONS */
void GDScriptLanguage::get_reserved_words(Vector<String> *p_words) const {

    static const char *_reserved_words[] = {
        // operators
        "and",
        "in",
        "not",
        "or",
        // types and values
        "false",
        "float",
        "int",
        "bool",
        "null",
        "PI",
        "TAU",
        "INF",
        "NAN",
        "self",
        "true",
        "void",
        // functions
        "as",
        "assert",
        "breakpoint",
        "class",
        "class_name",
        "extends",
        "is",
        "func",
        "preload",
        "setget",
        "signal",
        "tool",
        "yield",
        // var
        "const",
        "enum",
        "export",
        "onready",
        "static",
        "var",
        // control flow
        "break",
        "continue",
        "if",
        "elif",
        "else",
        "for",
        "pass",
        "return",
        "match",
        "while",
        "remote",
        "sync",
        "master",
        "puppet",
        "slave",
        "remotesync",
        "mastersync",
        "puppetsync",
        nullptr
    };

    const char **w = _reserved_words;

    while (*w) {

        p_words->push_back((*w));
        w++;
    }

    for (int i = 0; i < GDScriptFunctions::FUNC_MAX; i++) {
        p_words->push_back((GDScriptFunctions::get_func_name(GDScriptFunctions::Function(i))));
    }
}

bool GDScriptLanguage::handles_global_class_type(StringView p_type) const {

    return p_type.compare("GDScript")==0;
}

StringName GDScriptLanguage::get_global_class_name(StringView p_path, String *r_base_type, String *r_icon_path) const {

    PoolVector<uint8_t> sourcef;
    Error err;
    FileAccessRef f = FileAccess::open(p_path, FileAccess::READ, &err);
    if (err) {
        return StringName();
    }

    String source(f->get_as_utf8_string());

    GDScriptParser parser;
    parser.parse(source, PathUtils::get_base_dir(p_path), true, p_path, false, nullptr, true);

    if (parser.get_parse_tree() && parser.get_parse_tree()->type == GDScriptParser::Node::TYPE_CLASS) {

        const GDScriptParser::ClassNode *c = static_cast<const GDScriptParser::ClassNode *>(parser.get_parse_tree());
        if (r_icon_path) {
            if (c->icon_path.empty() || PathUtils::is_abs_path(c->icon_path))
                *r_icon_path = c->icon_path;
            else if (PathUtils::is_rel_path(c->icon_path))
                *r_icon_path = PathUtils::simplify_path(PathUtils::plus_file(PathUtils::get_base_dir(p_path),c->icon_path));
        }
        if (r_base_type) {

            const GDScriptParser::ClassNode *subclass = c;
            String path(p_path);
            GDScriptParser subparser;
            while (subclass) {
                if (subclass->extends_used) {
                    if (subclass->extends_file) {
                        if (subclass->extends_class.empty()) {
                            get_global_class_name(subclass->extends_file.asCString(), r_base_type);
                            subclass = nullptr;
                            break;
                        } else {
                            Vector<StringName> extend_classes(subclass->extends_class);

                            FileAccessRef subfile = FileAccess::open(subclass->extends_file.asCString(), FileAccess::READ);
                            if (!subfile) {
                                break;
                            }
                            String subsource(subfile->get_as_utf8_string());

                            if (subsource.empty()) {
                                break;
                            }
                            String subpath = subclass->extends_file.asCString();
                            if (PathUtils::is_rel_path(subpath)) {
                                subpath = PathUtils::simplify_path(PathUtils::plus_file(PathUtils::get_base_dir(path),subpath));
                            }

                            if (OK != subparser.parse(subsource, PathUtils::get_base_dir(subpath), true, subpath, false, nullptr, true)) {
                                break;
                            }
                            path = subpath;
                            if (!subparser.get_parse_tree() || subparser.get_parse_tree()->type != GDScriptParser::Node::TYPE_CLASS) {
                                break;
                            }
                            subclass = static_cast<const GDScriptParser::ClassNode *>(subparser.get_parse_tree());

                            while (!extend_classes.empty()) {
                                bool found = false;
                                for (int i = 0; i < subclass->subclasses.size(); i++) {
                                    const GDScriptParser::ClassNode *inner_class = subclass->subclasses[i];
                                    if (inner_class->name == extend_classes.front()) {
                                        extend_classes.pop_front();
                                        found = true;
                                        subclass = inner_class;
                                        break;
                                    }
                                }
                                if (!found) {
                                    subclass = nullptr;
                                    break;
                                }
                            }
                        }
                    } else if (subclass->extends_class.size() == 1) {
                        *r_base_type = subclass->extends_class[0].asCString();
                        subclass = nullptr;
                    } else {
                        break;
                    }
                } else {
                    *r_base_type = "RefCounted";
                    subclass = nullptr;
                }
            }
        }
        return c->name;
    }

    return StringName();
}

#ifdef DEBUG_ENABLED
String GDScriptWarning::get_message() const {

#define CHECK_SYMBOLS(m_amount) ERR_FAIL_COND_V(symbols.size() < m_amount, String());
#define CHECK_SYMBOLS_EMPTY() ERR_FAIL_COND_V(symbols.empty(), String());
    switch (code) {
        case UNASSIGNED_VARIABLE_OP_ASSIGN: {
            CHECK_SYMBOLS_EMPTY()
            return "Using assignment with operation but the variable '" + symbols[0] + "' was not previously assigned a value.";
        }
        case UNASSIGNED_VARIABLE: {
            CHECK_SYMBOLS_EMPTY()
            return "The variable '" + symbols[0] + "' was used but never assigned a value.";
        }
        case UNUSED_VARIABLE: {
            CHECK_SYMBOLS(1);
            return "The local variable '" + symbols[0] + "' is declared but never used in the block. If this is intended, prefix it with an underscore: '_" + symbols[0] + "'";
        }
        case SHADOWED_VARIABLE: {
            CHECK_SYMBOLS(2);
            return "The local variable '" + symbols[0] + "' is shadowing an already-defined variable at line " + symbols[1] + ".";
        }
        case UNUSED_CLASS_VARIABLE: {
            CHECK_SYMBOLS(1);
            return "The class variable '" + symbols[0] + "' is declared but never used in the script.";
        }
        case UNUSED_ARGUMENT: {
            CHECK_SYMBOLS(2);
            return "The argument '" + symbols[1] + "' is never used in the function '" + symbols[0] + "'. If this is intended, prefix it with an underscore: '_" + symbols[1] + "'";
        }
        case UNREACHABLE_CODE: {
            CHECK_SYMBOLS_EMPTY()
            return "Unreachable code (statement after return) in function '" + symbols[0] + "()'.";
        }
        case STANDALONE_EXPRESSION: {
            return "Standalone expression (the line has no effect).";
        }
        case VOID_ASSIGNMENT: {
            CHECK_SYMBOLS_EMPTY()
            return "Assignment operation, but the function '" + symbols[0] + "()' returns void.";
        }
        case NARROWING_CONVERSION: {
            return "Narrowing conversion (float is converted to int and loses precision).";
        }
        case FUNCTION_MAY_YIELD: {
            CHECK_SYMBOLS_EMPTY()
            return "Assigned variable is typed but the function '" + symbols[0] + "()' may yield and return a GDScriptFunctionState instead.";
        }
        case VARIABLE_CONFLICTS_FUNCTION: {
            CHECK_SYMBOLS_EMPTY()
            return "Variable declaration of '" + symbols[0] + "' conflicts with a function of the same name.";
        }
        case FUNCTION_CONFLICTS_VARIABLE: {
            CHECK_SYMBOLS_EMPTY()
            return "Function declaration of '" + symbols[0] + "()' conflicts with a variable of the same name.";
        }
        case FUNCTION_CONFLICTS_CONSTANT: {
            CHECK_SYMBOLS_EMPTY()
            return "Function declaration of '" + symbols[0] + "()' conflicts with a constant of the same name.";
        }
        case INCOMPATIBLE_TERNARY: {
            return ("Values of the ternary conditional are not mutually compatible.");
        }
        case UNUSED_SIGNAL: {
            CHECK_SYMBOLS_EMPTY()
            return "The signal '" + symbols[0] + "' is declared but never emitted.";
        }
        case RETURN_VALUE_DISCARDED: {
            CHECK_SYMBOLS_EMPTY();
            return "The function '" + symbols[0] + "()' returns a value, but this value is never used.";
        }
        case PROPERTY_USED_AS_FUNCTION: {
            CHECK_SYMBOLS(2);
            return "The method '" + symbols[0] + "()' was not found in base '" + symbols[1] + "' but there's a property with the same name. Did you mean to access it?";
        }
        case CONSTANT_USED_AS_FUNCTION: {
            CHECK_SYMBOLS(2)
            return "The method '" + symbols[0] + "()' was not found in base '" + symbols[1] + "' but there's a constant with the same name. Did you mean to access it?";
        }
        case FUNCTION_USED_AS_PROPERTY: {
            CHECK_SYMBOLS(2)
            return "The property '" + symbols[0] + "' was not found in base '" + symbols[1] + "' but there's a method with the same name. Did you mean to call it?";
        }
        case INTEGER_DIVISION: {
            return ("Integer division, decimal part will be discarded.");
        }
        case UNSAFE_PROPERTY_ACCESS: {
            CHECK_SYMBOLS(2)
            return "The property '" + symbols[0] + "' is not present on the inferred type '" + symbols[1] + "' (but may be present on a subtype).";
        }
        case UNSAFE_METHOD_ACCESS: {
            CHECK_SYMBOLS(2)
            return "The method '" + symbols[0] + "' is not present on the inferred type '" + symbols[1] + "' (but may be present on a subtype).";
        }
        case UNSAFE_CAST: {
            CHECK_SYMBOLS_EMPTY()
            return "The value is cast to '" + symbols[0] + "' but has an unknown type.";
        }
        case UNSAFE_CALL_ARGUMENT: {
            CHECK_SYMBOLS(4)
            return "The argument '" + symbols[0] + "' of the function '" + symbols[1] + "' requires a the subtype '" + symbols[2] + "' but the supertype '" + symbols[3] + "' was provided";
        }
        case DEPRECATED_KEYWORD: {
            CHECK_SYMBOLS(2)
            return "The '" + symbols[0] + "' keyword is deprecated and will be removed in a future release, please replace its uses by '" + symbols[1] + "'.";
        }
        case STANDALONE_TERNARY: {
            return "Standalone ternary conditional operator: the return value is being discarded.";
        }
        case WARNING_MAX: break; // Can't happen, but silences warning
    }
    ERR_FAIL_V_MSG(String(), String("Invalid GDScript warning code: ") + get_name_from_code(code) + ".");

#undef CHECK_SYMBOLS
}

String GDScriptWarning::get_name() const {
    return get_name_from_code(code);
}

const char *GDScriptWarning::get_name_from_code(Code p_code) {
    ERR_FAIL_COND_V(p_code < 0 || p_code >= WARNING_MAX, nullptr);

    static const char *names[] = {
        "UNASSIGNED_VARIABLE",
        "UNASSIGNED_VARIABLE_OP_ASSIGN",
        "UNUSED_VARIABLE",
        "SHADOWED_VARIABLE",
        "UNUSED_CLASS_VARIABLE",
        "UNUSED_ARGUMENT",
        "UNREACHABLE_CODE",
        "STANDALONE_EXPRESSION",
        "VOID_ASSIGNMENT",
        "NARROWING_CONVERSION",
        "FUNCTION_MAY_YIELD",
        "VARIABLE_CONFLICTS_FUNCTION",
        "FUNCTION_CONFLICTS_VARIABLE",
        "FUNCTION_CONFLICTS_CONSTANT",
        "INCOMPATIBLE_TERNARY",
        "UNUSED_SIGNAL",
        "RETURN_VALUE_DISCARDED",
        "PROPERTY_USED_AS_FUNCTION",
        "CONSTANT_USED_AS_FUNCTION",
        "FUNCTION_USED_AS_PROPERTY",
        "INTEGER_DIVISION",
        "UNSAFE_PROPERTY_ACCESS",
        "UNSAFE_METHOD_ACCESS",
        "UNSAFE_CAST",
        "UNSAFE_CALL_ARGUMENT",
        "DEPRECATED_KEYWORD",
        "STANDALONE_TERNARY",
        nullptr
    };

    return names[(int)p_code];
}

GDScriptWarning::Code GDScriptWarning::get_code_from_name(const String &p_name) {
    for (int i = 0; i < WARNING_MAX; i++) {
        if (get_name_from_code((Code)i) == p_name) {
            return (Code)i;
        }
    }

    ERR_FAIL_V_MSG(WARNING_MAX, "Invalid GDScript warning name: " + p_name);
}

#endif // DEBUG_ENABLED

GDScriptLanguage::GDScriptLanguage() {

    calls = 0;
    ERR_FAIL_COND(singleton);
    singleton = this;
    strings._init = StringName("_init");
    strings._notification = StringName("_notification");
    strings._set = StringName("_set");
    strings._get = StringName("_get");
    strings._get_property_list = StringName("_get_property_list");
    strings._script_source = StringName("script/source");
    _debug_parse_err_line = -1;
    _debug_parse_err_file = "";

    lock = memnew(Mutex);
    profiling = false;
    script_frame_time = 0;

    _debug_call_stack_pos = 0;
    int dmcs = GLOBAL_DEF("debug/settings/gdscript/max_call_stack", 1024);
    ProjectSettings::get_singleton()->set_custom_property_info("debug/settings/gdscript/max_call_stack", PropertyInfo(VariantType::INT, "debug/settings/gdscript/max_call_stack", PropertyHint::Range, "1024,4096,1,or_greater")); //minimum is 1024

    if (ScriptDebugger::get_singleton()) {
        //debugging enabled!

        _debug_max_call_stack = dmcs;
        _call_stack = memnew_arr(CallLevel, _debug_max_call_stack + 1);

    } else {
        _debug_max_call_stack = 0;
        _call_stack = nullptr;
    }

#ifdef DEBUG_ENABLED
    GLOBAL_DEF("debug/gdscript/warnings/enable", true);
    GLOBAL_DEF("debug/gdscript/warnings/treat_warnings_as_errors", false);
    GLOBAL_DEF("debug/gdscript/warnings/exclude_addons", true);
    GLOBAL_DEF("debug/gdscript/completion/autocomplete_setters_and_getters", false);
    for (int i = 0; i < (int)GDScriptWarning::WARNING_MAX; i++) {
        String warning(StringUtils::to_lower(String(GDScriptWarning::get_name_from_code((GDScriptWarning::Code)i))));
        bool default_enabled = !StringUtils::begins_with(warning,"unsafe_") && i != GDScriptWarning::UNUSED_CLASS_VARIABLE;
        GLOBAL_DEF(StringName("debug/gdscript/warnings/" + warning), default_enabled);
    }
#endif // DEBUG_ENABLED
}

GDScriptLanguage::~GDScriptLanguage() {

    if (lock) {
        memdelete(lock);
        lock = nullptr;
    }
    if (_call_stack) {
        memdelete_arr(_call_stack);
    }
    singleton = nullptr;
}
void GDScriptLanguage::add_orphan_subclass(const String &p_qualified_name, const ObjectID &p_subclass) {
    orphan_subclasses[p_qualified_name] = p_subclass;
}

Ref<GDScript> GDScriptLanguage::get_orphan_subclass(const String &p_qualified_name) {
    auto orphan_subclass_element = orphan_subclasses.find(p_qualified_name);
    if (orphan_subclasses.end()==orphan_subclass_element)
        return Ref<GDScript>();
    ObjectID orphan_subclass = orphan_subclass_element->second;
    Object *obj = gObjectDB().get_instance(orphan_subclass);
    orphan_subclasses.erase(orphan_subclass_element);
    if (!obj)
        return Ref<GDScript>();
    return Ref<GDScript>(object_cast<GDScript>(obj));
}
/*************** RESOURCE ***************/

RES ResourceFormatLoaderGDScript::load(StringView p_path, StringView p_original_path, Error *r_error) {

    if (r_error)
        *r_error = ERR_FILE_CANT_OPEN;

    GDScript *script = memnew(GDScript);

    Ref<GDScript> scriptres(script);

    if (StringUtils::ends_with(p_path,".gde") || StringUtils::ends_with(p_path,".gdc")) {

        script->set_script_path(p_original_path); // script needs this.
        script->set_path(p_original_path);
        Error err = script->load_byte_code(p_path);
        ERR_FAIL_COND_V_MSG(err != OK, RES(), "Cannot load byte code from file '" + String(p_path) + "'.");

    } else {
        Error err = script->load_source_code(p_path);
        ERR_FAIL_COND_V_MSG(err != OK, RES(), "Cannot load source code from file '" + String(p_path) + "'.");

        script->set_script_path(p_original_path); // script needs this.
        script->set_path(p_original_path);

        script->reload();
    }
    if (r_error)
        *r_error = OK;

    return scriptres;
}

void ResourceFormatLoaderGDScript::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.push_back("gd");
    p_extensions.push_back("gdc");
    p_extensions.push_back("gde");
}

bool ResourceFormatLoaderGDScript::handles_type(StringView p_type) const {

    return (p_type == "Script"_sv || p_type == "GDScript"_sv);
}

String ResourceFormatLoaderGDScript::get_resource_type(StringView p_path) const {

    String el = StringUtils::to_lower(PathUtils::get_extension(p_path));
    if (el == "gd" || el == "gdc" || el == "gde")
        return "GDScript";
    return {};
}

void ResourceFormatLoaderGDScript::get_dependencies(StringView p_path, Vector<String> &p_dependencies, bool p_add_types) {

    FileAccessRef file = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_MSG(!file, "Cannot open file '" + String(p_path) + "'.");

    String source(file->get_as_utf8_string());
    if (source.empty()) {
        return;
    }

    GDScriptParser parser;
    if (OK != parser.parse(source, PathUtils::get_base_dir(p_path), true, p_path, false, nullptr, true)) {
        return;
    }
    p_dependencies.push_back(parser.get_dependencies());
}

Error ResourceFormatSaverGDScript::save(StringView p_path, const RES &p_resource, uint32_t p_flags) {

    Ref<GDScript> sqscr = dynamic_ref_cast<GDScript>(p_resource);
    ERR_FAIL_COND_V(not sqscr, ERR_INVALID_PARAMETER);

    String source(sqscr->get_source_code());

    Error err;
    FileAccess *file = FileAccess::open(p_path, FileAccess::WRITE, &err);

    ERR_FAIL_COND_V_MSG(err, err, "Cannot save GDScript file '" + String(p_path) + "'.");

    file->store_string(source);
    if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
        memdelete(file);
        return ERR_CANT_CREATE;
    }
    file->close();
    memdelete(file);

    if (ScriptServer::is_reload_scripts_on_save_enabled()) {
        GDScriptLanguage::get_singleton()->reload_tool_script(dynamic_ref_cast<Script>(p_resource), false);
    }

    return OK;
}

void ResourceFormatSaverGDScript::get_recognized_extensions(const RES &p_resource, Vector<String> &p_extensions) const {

    if (object_cast<GDScript>(p_resource.get())) {
        p_extensions.push_back("gd");
    }
}
bool ResourceFormatSaverGDScript::recognize(const RES &p_resource) const {

    return object_cast<GDScript>(p_resource.get()) != nullptr;
}
