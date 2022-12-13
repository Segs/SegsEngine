/*************************************************************************/
/*  pluginscript_instance.h                                              */
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

#ifndef PLUGINSCRIPT_INSTANCE_H
#define PLUGINSCRIPT_INSTANCE_H

// Godot imports
#include "core/script_language.h"
// PluginScript imports
#include <pluginscript/godot_pluginscript.h>

class PluginScript;

class PluginScriptInstance : public ScriptInstance {
    friend class PluginScript;

private:
    Ref<PluginScript> _script;
    Object *_owner;
    Variant _owner_variant;
    godot_pluginscript_instance_data *_data;
    const godot_pluginscript_instance_desc *_desc;

public:
    _FORCE_INLINE_ Object *get_owner() override { return _owner; }

    bool set(const StringName &p_name, const Variant &p_value) override;
    bool get(const StringName &p_name, Variant &r_ret) const override;
    void get_property_list(List<PropertyInfo> *p_properties) const override;
    VariantType get_property_type(const StringName &p_name, bool *r_is_valid = nullptr) const override;

    void get_method_list(Vector<MethodInfo> *p_list) const override;
    bool has_method(const StringName &p_method) const override;

    Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;
#if 0
    // Rely on default implementations provided by ScriptInstance for the moment.
    // Note that multilevel call could be removed in 3.0 release, so stay tuned
    // (see https://godotengine.org/qa/9244/can-override-the-_ready-and-_process-functions-child-classes)
    virtual void call_multilevel(const StringName& p_method,const Variant** p_args,int p_argcount);
    virtual void call_multilevel_reversed(const StringName& p_method,const Variant** p_args,int p_argcount);
#endif

    void notification(int p_notification) override;

    Ref<Script> get_script() const override;

    ScriptLanguage *get_language() override;

    void set_path(StringView p_path);

    MultiplayerAPI_RPCMode get_rpc_mode(const StringName &p_method) const override;
    MultiplayerAPI_RPCMode get_rset_mode(const StringName &p_variable) const override;

    void refcount_incremented() override;
    bool refcount_decremented() override;

    PluginScriptInstance();
    bool init(PluginScript *p_script, Object *p_owner);
    ~PluginScriptInstance() override;
};

#endif // PLUGINSCRIPT_INSTANCE_H
