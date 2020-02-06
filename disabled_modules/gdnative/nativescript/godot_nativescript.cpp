/*************************************************************************/
/*  godot_nativescript.cpp                                               */
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

#include "nativescript/godot_nativescript.h"

#include "core/class_db.h"
#include "core/error_macros.h"
#include "core/global_constants.h"
#include "core/project_settings.h"
#include "core/variant.h"
#include "gdnative/gdnative.h"

#include "nativescript.h"

#ifdef __cplusplus
extern "C" {
#endif

extern "C" void _native_script_hook() {
}

#define NSL NativeScriptLanguage::get_singleton()

// Script API

void GDAPI godot_nativescript_register_class(void *p_gdnative_handle, const char *p_name, const char *p_base, godot_instance_create_func p_create_func, godot_instance_destroy_func p_destroy_func) {

    String *s = (String *)p_gdnative_handle;

    Map<StringName, NativeScriptDesc> *classes = &NSL->library_classes[*s];

    NativeScriptDesc desc;

    desc.create_func = p_create_func;
    desc.destroy_func = p_destroy_func;
    desc.is_tool = false;

    desc.base = StringName(p_base);

    if (classes->contains(desc.base)) {
        desc.base_data = &(*classes)[desc.base];
        desc.base_native_type = desc.base_data->base_native_type;
    } else {
        desc.base_data = nullptr;
        desc.base_native_type = desc.base;
    }

    classes->emplace(StringName(p_name), desc);
}

void GDAPI godot_nativescript_register_tool_class(void *p_gdnative_handle, const char *p_name, const char *p_base, godot_instance_create_func p_create_func, godot_instance_destroy_func p_destroy_func) {

    String *s = (String *)p_gdnative_handle;

    Map<StringName, NativeScriptDesc> *classes = &NSL->library_classes[*s];

    NativeScriptDesc desc;

    desc.create_func = p_create_func;
    desc.destroy_func = p_destroy_func;
    desc.is_tool = true;
    desc.base = StringName(p_base);

    if (classes->contains(desc.base)) {
        desc.base_data = &(*classes)[desc.base];
        desc.base_native_type = desc.base_data->base_native_type;
    } else {
        desc.base_data = nullptr;
        desc.base_native_type = desc.base;
    }

    classes->emplace(StringName(p_name), desc);
}

void GDAPI godot_nativescript_register_method(void *p_gdnative_handle, const char *p_name, const char *p_function_name, godot_method_attributes p_attr, godot_instance_method p_method) {

    String *s = (String *)p_gdnative_handle;
    auto &classes(NSL->library_classes[*s]);
    Map<StringName, NativeScriptDesc>::iterator E = classes.find(StringName(p_name));
    ERR_FAIL_COND_MSG(E==classes.end(), "Attempted to register method on non-existent class."); 

    NativeScriptDesc::Method method;
    method.method = p_method;
    method.rpc_mode = p_attr.rpc_type;
    method.info = MethodInfo(p_function_name);

    E->second.methods.emplace(StringName(p_function_name), method);
}

void GDAPI godot_nativescript_register_property(void *p_gdnative_handle, const char *p_name, const char *p_path, godot_property_attributes *p_attr, godot_property_set_func p_set_func, godot_property_get_func p_get_func) {

    String *s = (String *)p_gdnative_handle;
    auto &classes(NSL->library_classes[*s]);
    Map<StringName, NativeScriptDesc>::iterator E = classes.find(StringName(p_name));
    ERR_FAIL_COND_MSG(E==classes.end(), "Attempted to register method on non-existent class."); 

    NativeScriptDesc::Property property;
    property.default_value = *(Variant *)&p_attr->default_value;
    property.getter = p_get_func;
    property.rset_mode = p_attr->rset_type;
    property.setter = p_set_func;
    property.info = PropertyInfo((VariantType)p_attr->type,
            StringName(p_path),
            (PropertyHint)p_attr->hint,
            *(String *)&p_attr->hint_string,
            (PropertyUsageFlags)p_attr->usage);

    E->second.properties.insert(StringName(p_path), property);
}

void GDAPI godot_nativescript_register_signal(void *p_gdnative_handle, const char *p_name, const godot_signal *p_signal) {

    String *s = (String *)p_gdnative_handle;
    auto &classes(NSL->library_classes[*s]);
    Map<StringName, NativeScriptDesc>::iterator E = classes.find(StringName(p_name));
    ERR_FAIL_COND_MSG(E==classes.end(), "Attempted to register method on non-existent class."); 

    PODVector<PropertyInfo> args;
    PODVector<Variant> default_args;

    args.reserve(p_signal->num_args);

    for (int i = 0; i < p_signal->num_args; i++) {
        PropertyInfo info;

        godot_signal_argument arg = p_signal->args[i];

        info.hint = (PropertyHint)arg.hint;
        info.hint_string = *(String *)&arg.hint_string;
        info.name = StringName(*(String *)&arg.name);
        info.type = (VariantType)arg.type;
        info.usage = (PropertyUsageFlags)arg.usage;

        args.push_back(info);
    }

    default_args.reserve(p_signal->num_default_args);
    for (int i = 0; i < p_signal->num_default_args; i++) {
        Variant *v;
        godot_signal_argument attrib = p_signal->args[i];

        v = (Variant *)&attrib.default_value;

        default_args.push_back(*v);
    }

    MethodInfo method_info;
    method_info.name = StringName(*(String *)&p_signal->name);
    method_info.arguments = std::move(args);
    method_info.default_arguments = std::move(default_args);

    NativeScriptDesc::Signal signal;
    signal.signal = method_info;

    E->second.signals_.emplace(StringName(*(String *)&p_signal->name), signal);
}

void GDAPI *godot_nativescript_get_userdata(godot_object *p_instance) {
    Object *instance = (Object *)p_instance;
    if (!instance)
        return nullptr;
    if (instance->get_script_instance() && instance->get_script_instance()->get_language() == NativeScriptLanguage::get_singleton()) {
        return ((NativeScriptInstance *)instance->get_script_instance())->userdata;
    }
    return nullptr;
}

/*
 *
 *
 * NativeScript 1.1
 *
 *
 */

void GDAPI godot_nativescript_set_method_argument_information(void *p_gdnative_handle, const char *p_name, const char *p_function_name, int p_num_args, const godot_method_arg *p_args) {
    String *s = (String *)p_gdnative_handle;
    auto &classes(NSL->library_classes[*s]);
    Map<StringName, NativeScriptDesc>::iterator E = classes.find(StringName(p_name));
    ERR_FAIL_COND_MSG(E==classes.end(), "Attempted to add argument information for a method on a non-existent class."); 

    Map<StringName, NativeScriptDesc::Method>::iterator method = E->second.methods.find(StringName(p_function_name));
    ERR_FAIL_COND_MSG(method==E->second.methods.end(), "Attempted to add argument information to non-existent method."); 

    MethodInfo *method_information = &method->second.info;

    PODVector<PropertyInfo> args;
    args.reserve(p_num_args);

    for (int i = 0; i < p_num_args; i++) {
        godot_method_arg arg = p_args[i];
        String name = *(String *)&arg.name;
        String hint_string = *(String *)&arg.hint_string;

        VariantType type = (VariantType)arg.type;
        PropertyHint hint = (PropertyHint)arg.hint;

        args.push_back(PropertyInfo(type, StringName(p_name), hint, hint_string));
    }

    method_information->arguments = args;
}

void GDAPI godot_nativescript_set_class_documentation(void *p_gdnative_handle, const char *p_name, godot_string p_documentation) {
    String *s = (String *)p_gdnative_handle;
    auto &classes(NSL->library_classes[*s]);

    Map<StringName, NativeScriptDesc>::iterator E = classes.find(StringName(p_name));
    ERR_FAIL_COND_MSG(E==classes.end(), "Attempted to add documentation to a non-existent class."); 

    E->second.documentation = *(String *)&p_documentation;
}

void GDAPI godot_nativescript_set_method_documentation(void *p_gdnative_handle, const char *p_name, const char *p_function_name, godot_string p_documentation) {
    String *s = (String *)p_gdnative_handle;
    auto &classes(NSL->library_classes[*s]);

    Map<StringName, NativeScriptDesc>::iterator E = classes.find(StringName(p_name));
    ERR_FAIL_COND_MSG(E==classes.end(), "Attempted to add documentation to a method on a non-existent class."); 

    Map<StringName, NativeScriptDesc::Method>::iterator method = E->second.methods.find(StringName(p_function_name));
    ERR_FAIL_COND_MSG(method==E->second.methods.end(), "Attempted to add documentation to non-existent method."); 

    method->second.documentation = *(String *)&p_documentation;
}

void GDAPI godot_nativescript_set_property_documentation(void *p_gdnative_handle, const char *p_name, const char *p_path, godot_string p_documentation) {
    String *s = (String *)p_gdnative_handle;
    auto &classes(NSL->library_classes[*s]);

    Map<StringName, NativeScriptDesc>::iterator E = classes.find(StringName(p_name));
    ERR_FAIL_COND_MSG(E==classes.end(), "Attempted to add documentation to a property on a non-existent class."); 

    OrderedHashMap<StringName, NativeScriptDesc::Property>::Element property = E->second.properties.find(StringName(p_path));
    ERR_FAIL_COND_MSG(!property, "Attempted to add documentation to non-existent property."); 

    property.get().documentation = *(String *)&p_documentation;
}

void GDAPI godot_nativescript_set_signal_documentation(void *p_gdnative_handle, const char *p_name, const char *p_signal_name, godot_string p_documentation) {
    String *s = (String *)p_gdnative_handle;
    auto &classes(NSL->library_classes[*s]);
    Map<StringName, NativeScriptDesc>::iterator E = classes.find(StringName(p_name));
    ERR_FAIL_COND_MSG(E==classes.end(), "Attempted to add documentation to a signal on a non-existent class."); 

    Map<StringName, NativeScriptDesc::Signal>::iterator signal = E->second.signals_.find(StringName(p_signal_name));
    ERR_FAIL_COND_MSG(signal==E->second.signals_.end(), "Attempted to add documentation to non-existent signal."); 

    signal->second.documentation = *(String *)&p_documentation;
}

void GDAPI godot_nativescript_set_global_type_tag(int p_idx, const char *p_name, const void *p_type_tag) {
    NativeScriptLanguage::get_singleton()->set_global_type_tag(p_idx, StringName(p_name), p_type_tag);
}

const void GDAPI *godot_nativescript_get_global_type_tag(int p_idx, const char *p_name) {
    return NativeScriptLanguage::get_singleton()->get_global_type_tag(p_idx, StringName(p_name));
}

void GDAPI godot_nativescript_set_type_tag(void *p_gdnative_handle, const char *p_name, const void *p_type_tag) {
    String *s = (String *)p_gdnative_handle;
    auto &classes(NSL->library_classes[*s]);

    Map<StringName, NativeScriptDesc>::iterator E = classes.find(StringName(p_name));
    ERR_FAIL_COND_MSG(E==classes.end(), "Attempted to set type tag on a non-existent class."); 

    E->second.type_tag = p_type_tag;
}

const void GDAPI *godot_nativescript_get_type_tag(const godot_object *p_object) {

    const Object *o = (Object *)p_object;

    if (!o->get_script_instance()) {
        return nullptr;
    } else {
        NativeScript *script = object_cast<NativeScript>(o->get_script_instance()->get_script().get());
        if (!script) {
            return nullptr;
        }

        if (script->get_script_desc())
            return script->get_script_desc()->type_tag;
    }

    return nullptr;
}

int GDAPI godot_nativescript_register_instance_binding_data_functions(godot_instance_binding_functions p_binding_functions) {
    return NativeScriptLanguage::get_singleton()->register_binding_functions(p_binding_functions);
}

void GDAPI godot_nativescript_unregister_instance_binding_data_functions(int p_idx) {
    NativeScriptLanguage::get_singleton()->unregister_binding_functions(p_idx);
}

void GDAPI *godot_nativescript_get_instance_binding_data(int p_idx, godot_object *p_object) {
    return NativeScriptLanguage::get_singleton()->get_instance_binding_data(p_idx, (Object *)p_object);
}

void GDAPI godot_nativescript_profiling_add_data(const char *p_signature, uint64_t p_time) {
    NativeScriptLanguage::get_singleton()->profiling_add_data(StringName(p_signature), p_time);
}

#ifdef __cplusplus
}
#endif
