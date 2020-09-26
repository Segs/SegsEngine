/*************************************************************************/
/*  visual_script.cpp                                                    */
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

#include "visual_script.h"

#include "core/core_string_names.h"
#include "core/method_bind.h"
#include "core/object_db.h"
#include "core/object_tooling.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/string.h"
#include "core/translation_helpers.h"
#include "scene/main/node.h"
#include "visual_script_nodes.h"

#include "EASTL/sort.h"

IMPL_GDCLASS(VisualScript)
IMPL_GDCLASS(VisualScriptNode)
IMPL_GDCLASS(VisualScriptFunctionState)

RES_BASE_EXTENSION_IMPL(VisualScript,"vs")
//used by editor, this is not really saved
void VisualScriptNode::set_breakpoint(bool p_breakpoint) {
    breakpoint = p_breakpoint;
}

bool VisualScriptNode::is_breakpoint() const {

    return breakpoint;
}

void VisualScriptNode::ports_changed_notify() {
    emit_signal("ports_changed");
}

void VisualScriptNode::set_default_input_value(int p_port, const Variant &p_value) {

    ERR_FAIL_INDEX(p_port, default_input_values.size());

    default_input_values[p_port] = p_value;

    for (VisualScript *E : scripts_used) {
        Object_set_edited(E,true);
    }
}

Variant VisualScriptNode::get_default_input_value(int p_port) const {

    ERR_FAIL_INDEX_V(p_port, default_input_values.size(), Variant::null_variant);
    return default_input_values[p_port];
}

void VisualScriptNode::_set_default_input_values(Array p_values) {

    default_input_values = p_values;
}

void VisualScriptNode::validate_input_default_values() {
    default_input_values.resize(M_MAX(default_input_values.size(), get_input_value_port_count())); //let it grow as big as possible, we don't want to lose values on resize

    //actually validate on save
    for (int i = 0; i < get_input_value_port_count(); i++) {

        VariantType expected = get_input_value_port_info(i).type;

        if (expected == VariantType::NIL || expected == default_input_values[i].get_type()) {
            continue;
        } else {
            //not the same, reconvert
            Callable::CallError ce;
            Variant existing = default_input_values[i];
            const Variant *existingp = &existing;
            default_input_values[i] = Variant::construct(expected, &existingp, 1, ce, false);
            if (ce.error != Callable::CallError::CALL_OK) {
                //could not convert? force..
                default_input_values[i] = Variant::construct(expected, nullptr, 0, ce, false);
            }
        }
    }
}

Array VisualScriptNode::_get_default_input_values() const {

    //validate on save, since on load there is little info about this
    Array values = default_input_values;
    values.resize(get_input_value_port_count());

    return values;
}

String VisualScriptNode::get_text() const {
    return {};
}

void VisualScriptNode::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("get_visual_script"), &VisualScriptNode::get_visual_script);
    MethodBinder::bind_method(D_METHOD("set_default_input_value", {"port_idx", "value"}), &VisualScriptNode::set_default_input_value);
    MethodBinder::bind_method(D_METHOD("get_default_input_value", {"port_idx"}), &VisualScriptNode::get_default_input_value);
    MethodBinder::bind_method(D_METHOD("ports_changed_notify"), &VisualScriptNode::ports_changed_notify);
    MethodBinder::bind_method(D_METHOD("_set_default_input_values", {"values"}), &VisualScriptNode::_set_default_input_values);
    MethodBinder::bind_method(D_METHOD("_get_default_input_values"), &VisualScriptNode::_get_default_input_values);

    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "_default_input_values", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_default_input_values", "_get_default_input_values");
    ADD_SIGNAL(MethodInfo("ports_changed"));
}

VisualScriptNode::TypeGuess VisualScriptNode::guess_output_type(TypeGuess * /*p_inputs*/, int p_output) const {

    PropertyInfo pinfo = get_output_value_port_info(p_output);

    TypeGuess tg;

    tg.type = pinfo.type;
    if (pinfo.hint == PropertyHint::ResourceType) {
        tg.gdclass = StringName(pinfo.hint_string);
    }

    return tg;
}

Ref<VisualScript> VisualScriptNode::get_visual_script() const {

    if (!scripts_used.empty())
        return Ref<VisualScript>(*scripts_used.begin());

    return Ref<VisualScript>();
}

VisualScriptNode::VisualScriptNode() {
    breakpoint = false;
}

////////////////

/////////////////////

VisualScriptNodeInstance::VisualScriptNodeInstance() {

    sequence_outputs = nullptr;
    input_ports = nullptr;
}

VisualScriptNodeInstance::~VisualScriptNodeInstance() {

    if (sequence_outputs) {
        memdelete_arr(sequence_outputs);
    }

    if (input_ports) {
        memdelete_arr(input_ports);
    }

    if (output_ports) {
        memdelete_arr(output_ports);
    }
}

void VisualScript::add_function(const StringName &p_name) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!StringUtils::is_valid_identifier((p_name)));
    ERR_FAIL_COND(functions.contains(p_name));

    functions[p_name] = Function();
    functions[p_name].scroll = Vector2(-50, -100);
}

bool VisualScript::has_function(const StringName &p_name) const {

    return functions.contains(p_name);
}
void VisualScript::remove_function(const StringName &p_name) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!functions.contains(p_name));

    for (auto &E : functions[p_name].nodes) {

        E.second.node->disconnect("ports_changed", this, "_node_ports_changed");
        E.second.node->scripts_used.erase(this);
    }

    functions.erase(p_name);
}

void VisualScript::rename_function(const StringName &p_name, const StringName &p_new_name) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!functions.contains(p_name));
    if (p_new_name == p_name)
        return;

    ERR_FAIL_COND(!StringUtils::is_valid_identifier((p_new_name)));

    ERR_FAIL_COND(functions.contains(p_new_name));
    ERR_FAIL_COND(variables.contains(p_new_name));
    ERR_FAIL_COND(custom_signals.contains(p_new_name));

    functions[p_new_name] = functions[p_name];
    functions.erase(p_name);
}

void VisualScript::set_function_scroll(const StringName &p_name, const Vector2 &p_scroll) {

    ERR_FAIL_COND(!functions.contains(p_name));
    functions[p_name].scroll = p_scroll;
}

Vector2 VisualScript::get_function_scroll(const StringName &p_name) const {

    ERR_FAIL_COND_V(!functions.contains(p_name), Vector2());
    return functions.at(p_name).scroll;
}

void VisualScript::get_function_list(Vector<StringName> *r_functions) const {

    for (const eastl::pair<const StringName,Function> &E : functions) {
        r_functions->push_back(E.first);
    }
    eastl::sort(r_functions->begin(),r_functions->end(),StringName::AlphCompare);
}

int VisualScript::get_function_node_id(const StringName &p_name) const {

    ERR_FAIL_COND_V(!functions.contains(p_name), -1);

    return functions.at(p_name).function_id;
}

void VisualScript::_node_ports_changed(int p_id) {

    StringName function;

    for (eastl::pair<const StringName,Function> &E : functions) {

        if (E.second.nodes.contains(p_id)) {
            function = E.first;
            break;
        }
    }

    ERR_FAIL_COND(function == StringName());

    Function &func = functions[function];
    Ref<VisualScriptNode> vsn = func.nodes[p_id].node;

    vsn->validate_input_default_values();

    //must revalidate all the functions

    for (auto iter= func.sequence_connections.begin(); iter!= func.sequence_connections.end();  ) {
        if (iter->from_node == p_id && iter->from_output >= vsn->get_output_sequence_port_count()) {

            iter = func.sequence_connections.erase(iter);
        }
        else if (iter->to_node == p_id && !vsn->has_input_sequence_port()) {

            iter = func.sequence_connections.erase(iter);
        }
        else
            ++iter;
    }

    for (auto iter = func.data_connections.begin(); iter != func.data_connections.end(); ) {
        if (iter->from_node == p_id && iter->from_port >= vsn->get_output_value_port_count()) {
            iter = func.data_connections.erase(iter);
        }
        else if (iter->to_node == p_id && iter->to_port >= vsn->get_input_value_port_count()) {
            iter = func.data_connections.erase(iter);
        }
        else
            ++iter;
    }

#ifdef TOOLS_ENABLED
    Object_set_edited(this,true); //something changed, let's set as edited
    emit_signal("node_ports_changed", function, p_id);
#endif
}

void VisualScript::add_node(const StringName &p_func, int p_id, const Ref<VisualScriptNode> &p_node, const Point2 &p_pos) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!functions.contains(p_func));

    for (eastl::pair<const StringName,Function> &E : functions) {

        ERR_FAIL_COND(E.second.nodes.contains(p_id)); //id can exist only one in script, even for different functions
    }

    Function &func = functions[p_func];

    if (object_cast<VisualScriptFunction>(p_node.get())) {
        //the function indeed
        ERR_FAIL_COND_MSG(func.function_id >= 0, "A function node has already been set here.");

        func.function_id = p_id;
    }

    Function::NodeData nd;
    nd.node = p_node;
    nd.pos = p_pos;

    p_node->connect("ports_changed", this, "_node_ports_changed", varray(p_id));
    p_node->scripts_used.insert(this);
    p_node->validate_input_default_values(); // Validate when fully loaded

    func.nodes[p_id] = nd;
}

void VisualScript::remove_node(const StringName &p_func, int p_id) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!functions.contains(p_func));
    Function &func = functions[p_func];

    ERR_FAIL_COND(!func.nodes.contains(p_id));
    for (auto iter = func.sequence_connections.begin(); iter != func.sequence_connections.end(); ) {
        if (iter->from_node == p_id || iter->to_node == p_id) {
            iter = func.sequence_connections.erase(iter);
        }
        else
            ++iter;
    }

    for (auto iter = func.data_connections.begin(); iter != func.data_connections.end(); ) {
        if (iter->from_node == p_id || iter->to_node== p_id) {
            iter = func.data_connections.erase(iter);
        }
        else
            ++iter;
    }

    if (object_cast<VisualScriptFunction>(func.nodes[p_id].node.get())) {
        func.function_id = -1; //revert to invalid
    }

    func.nodes[p_id].node->disconnect("ports_changed", this, "_node_ports_changed");
    func.nodes[p_id].node->scripts_used.erase(this);

    func.nodes.erase(p_id);
}

bool VisualScript::has_node(const StringName &p_func, int p_id) const {

    ERR_FAIL_COND_V(!functions.contains(p_func), false);
    const Function &func = functions.at(p_func);

    return func.nodes.contains(p_id);
}

Ref<VisualScriptNode> VisualScript::get_node(const StringName &p_func, int p_id) const {

    ERR_FAIL_COND_V(!functions.contains(p_func), Ref<VisualScriptNode>());
    const Function &func = functions.at(p_func);

    ERR_FAIL_COND_V(!func.nodes.contains(p_id), Ref<VisualScriptNode>());

    return func.nodes.at(p_id).node;
}

void VisualScript::set_node_position(const StringName &p_func, int p_id, const Point2 &p_pos) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!functions.contains(p_func));
    Function &func = functions[p_func];

    ERR_FAIL_COND(!func.nodes.contains(p_id));
    func.nodes[p_id].pos = p_pos;
}

Point2 VisualScript::get_node_position(const StringName &p_func, int p_id) const {

    ERR_FAIL_COND_V(!functions.contains(p_func), Point2());
    const Function &func = functions.at(p_func);

    ERR_FAIL_COND_V(!func.nodes.contains(p_id), Point2());
    return func.nodes.at(p_id).pos;
}

void VisualScript::get_node_list(const StringName &p_func, Vector<int> *r_nodes) const {

    ERR_FAIL_COND(!functions.contains(p_func));
    const Function &func = functions.at(p_func);

    for (const eastl::pair<const int,Function::NodeData> &E : func.nodes) {
        r_nodes->push_back(E.first);
    }
}

void VisualScript::sequence_connect(const StringName &p_func, int p_from_node, int p_from_output, int p_to_node) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!functions.contains(p_func));
    Function &func = functions[p_func];

    SequenceConnection sc;
    sc.from_node = p_from_node;
    sc.from_output = p_from_output;
    sc.to_node = p_to_node;
    ERR_FAIL_COND(func.sequence_connections.contains(sc));

    func.sequence_connections.insert(sc);
}

void VisualScript::sequence_disconnect(const StringName &p_func, int p_from_node, int p_from_output, int p_to_node) {

    ERR_FAIL_COND(!functions.contains(p_func));
    Function &func = functions[p_func];

    SequenceConnection sc;
    sc.from_node = p_from_node;
    sc.from_output = p_from_output;
    sc.to_node = p_to_node;
    ERR_FAIL_COND(!func.sequence_connections.contains(sc));

    func.sequence_connections.erase(sc);
}

bool VisualScript::has_sequence_connection(const StringName &p_func, int p_from_node, int p_from_output, int p_to_node) const {

    ERR_FAIL_COND_V(!functions.contains(p_func), false);
    const Function &func = functions.at(p_func);

    SequenceConnection sc;
    sc.from_node = p_from_node;
    sc.from_output = p_from_output;
    sc.to_node = p_to_node;

    return func.sequence_connections.contains(sc);
}

void VisualScript::get_sequence_connection_list(const StringName &p_func, Vector<SequenceConnection> *r_connection) const {

    ERR_FAIL_COND(!functions.contains(p_func));
    const Function &func = functions.at(p_func);
    assert(r_connection->empty()); // verify hard precondition, if this fails we'll have to switch to using `append` below
    r_connection->assign(func.sequence_connections.begin(), func.sequence_connections.end());
}

void VisualScript::data_connect(const StringName &p_func, int p_from_node, int p_from_port, int p_to_node, int p_to_port) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!functions.contains(p_func));
    Function &func = functions[p_func];

    DataConnection dc;
    dc.from_node = p_from_node;
    dc.from_port = p_from_port;
    dc.to_node = p_to_node;
    dc.to_port = p_to_port;

    ERR_FAIL_COND(func.data_connections.contains(dc));

    func.data_connections.insert(dc);
}

void VisualScript::data_disconnect(const StringName &p_func, int p_from_node, int p_from_port, int p_to_node, int p_to_port) {

    ERR_FAIL_COND(!functions.contains(p_func));
    Function &func = functions[p_func];

    DataConnection dc;
    dc.from_node = p_from_node;
    dc.from_port = p_from_port;
    dc.to_node = p_to_node;
    dc.to_port = p_to_port;

    ERR_FAIL_COND(!func.data_connections.contains(dc));

    func.data_connections.erase(dc);
}

bool VisualScript::has_data_connection(const StringName &p_func, int p_from_node, int p_from_port, int p_to_node, int p_to_port) const {

    ERR_FAIL_COND_V(!functions.contains(p_func), false);
    const Function &func = functions.at(p_func);

    DataConnection dc;
    dc.from_node = p_from_node;
    dc.from_port = p_from_port;
    dc.to_node = p_to_node;
    dc.to_port = p_to_port;

    return func.data_connections.contains(dc);
}

bool VisualScript::is_input_value_port_connected(const StringName &p_func, int p_node, int p_port) const {

    ERR_FAIL_COND_V(!functions.contains(p_func), false);
    const Function &func = functions.at(p_func);

    for (const DataConnection &E : func.data_connections) {
        if (E.to_node == p_node && E.to_port == p_port)
            return true;
    }

    return false;
}

bool VisualScript::get_input_value_port_connection_source(const StringName &p_func, int p_node, int p_port, int *r_node, int *r_port) const {

    ERR_FAIL_COND_V(!functions.contains(p_func), false);
    const Function &func = functions.at(p_func);

    for (const DataConnection &E : func.data_connections) {
        if (E.to_node == p_node && E.to_port == p_port) {
            *r_node = E.from_node;
            *r_port = E.from_port;
            return true;
        }
    }

    return false;
}

void VisualScript::get_data_connection_list(const StringName &p_func, Vector<DataConnection> *r_connection) const {

    ERR_FAIL_COND(!functions.contains(p_func));
    const Function &func = functions.at(p_func);
    assert(r_connection->empty()); // verify hard precondition, if this fails we'll have to switch to using `append` below
    r_connection->assign(func.data_connections.begin(), func.data_connections.end());
}

void VisualScript::set_tool_enabled(bool p_enabled) {
    is_tool_script = p_enabled;
}

void VisualScript::add_variable(const StringName &p_name, const Variant &p_default_value, bool p_export) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!StringUtils::is_valid_identifier((p_name)));
    ERR_FAIL_COND(variables.contains(p_name));

    Variable v;
    v.default_value = p_default_value;
    v.info.type = p_default_value.get_type();
    v.info.name = p_name;
    v.info.hint = PropertyHint::None;
    v._export = p_export;

    variables[p_name] = v;

#ifdef TOOLS_ENABLED
    _update_placeholders();
#endif
}

bool VisualScript::has_variable(const StringName &p_name) const {

    return variables.contains(p_name);
}

void VisualScript::remove_variable(const StringName &p_name) {

    ERR_FAIL_COND(!variables.contains(p_name));
    variables.erase(p_name);

#ifdef TOOLS_ENABLED
    _update_placeholders();
#endif
}

void VisualScript::set_variable_default_value(const StringName &p_name, const Variant &p_value) {

    ERR_FAIL_COND(!variables.contains(p_name));

    variables[p_name].default_value = p_value;

#ifdef TOOLS_ENABLED
    _update_placeholders();
#endif
}
Variant VisualScript::get_variable_default_value(const StringName &p_name) const {

    ERR_FAIL_COND_V(!variables.contains(p_name), Variant());
    return variables.at(p_name).default_value;
}
void VisualScript::set_variable_info(const StringName &p_name, const PropertyInfo &p_info) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!variables.contains(p_name));
    variables[p_name].info = p_info;
    variables[p_name].info.name = p_name;

#ifdef TOOLS_ENABLED
    _update_placeholders();
#endif
}
PropertyInfo VisualScript::get_variable_info(const StringName &p_name) const {

    ERR_FAIL_COND_V(!variables.contains(p_name), PropertyInfo());
    return variables.at(p_name).info;
}

void VisualScript::set_variable_export(const StringName &p_name, bool p_export) {

    ERR_FAIL_COND(!variables.contains(p_name));

    variables[p_name]._export = p_export;
#ifdef TOOLS_ENABLED
    _update_placeholders();
#endif
}

bool VisualScript::get_variable_export(const StringName &p_name) const {

    ERR_FAIL_COND_V(!variables.contains(p_name), false);
    return variables.at(p_name)._export;
}

void VisualScript::_set_variable_info(const StringName &p_name, const Dictionary &p_info) {

    PropertyInfo pinfo;
    if (p_info.has("type"))
        pinfo.type = p_info["type"].as<VariantType>();
    if (p_info.has("name"))
        pinfo.name = p_info["name"].as<StringName>();
    if (p_info.has("hint"))
        pinfo.hint = p_info["hint"].as<PropertyHint>();
    if (p_info.has("hint_string"))
        pinfo.hint_string = p_info["hint_string"].as<String>();
    if (p_info.has("usage"))
        pinfo.usage = p_info["usage"].as<uint32_t>();

    set_variable_info(p_name, pinfo);
}

Dictionary VisualScript::_get_variable_info(const StringName &p_name) const {

    PropertyInfo pinfo = get_variable_info(p_name);
    Dictionary d;
    d["type"] = int(pinfo.type);
    d["name"] = pinfo.name;
    d["hint"] = pinfo.hint;
    d["hint_string"] = pinfo.hint_string;
    d["usage"] = pinfo.usage;

    return d;
}

void VisualScript::get_variable_list(Vector<StringName> *r_variables) const {

    for (const eastl::pair<const StringName,Variable> &E : variables) {
        r_variables->push_back(E.first);
    }
    eastl::sort(r_variables->begin(),r_variables->end(),StringName::AlphCompare);
}

void VisualScript::set_instance_base_type(const StringName &p_type) {

    ERR_FAIL_COND(instances.size());
    base_type = p_type;
}

void VisualScript::rename_variable(const StringName &p_name, const StringName &p_new_name) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!variables.contains(p_name));
    if (p_new_name == p_name)
        return;

    ERR_FAIL_COND(!StringUtils::is_valid_identifier((p_new_name)));

    ERR_FAIL_COND(functions.contains(p_new_name));
    ERR_FAIL_COND(variables.contains(p_new_name));
    ERR_FAIL_COND(custom_signals.contains(p_new_name));

    variables[p_new_name] = variables[p_name];
    variables.erase(p_name);
}

void VisualScript::add_custom_signal(const StringName &p_name) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!StringUtils::is_valid_identifier((p_name)));
    ERR_FAIL_COND(custom_signals.contains(p_name));

    custom_signals[p_name].clear();
}

bool VisualScript::has_custom_signal(const StringName &p_name) const {

    return custom_signals.contains(p_name);
}
void VisualScript::custom_signal_add_argument(const StringName &p_func, VariantType p_type, const StringName &p_name, int p_index) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!custom_signals.contains(p_func));
    Argument arg;
    arg.type = p_type;
    arg.name = p_name;
    if (p_index < 0)
        custom_signals[p_func].push_back(arg);
    else
        custom_signals[p_func].insert(0, arg);
}
void VisualScript::custom_signal_set_argument_type(const StringName &p_func, int p_argidx, VariantType p_type) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!custom_signals.contains(p_func));
    ERR_FAIL_INDEX(p_argidx, custom_signals[p_func].size());
    custom_signals[p_func][p_argidx].type = p_type;
}
VariantType VisualScript::custom_signal_get_argument_type(const StringName &p_func, int p_argidx) const {

    ERR_FAIL_COND_V(!custom_signals.contains(p_func), VariantType::NIL);
    ERR_FAIL_INDEX_V(p_argidx, custom_signals.at(p_func).size(), VariantType::NIL);
    return custom_signals.at(p_func)[p_argidx].type;
}
void VisualScript::custom_signal_set_argument_name(const StringName &p_func, int p_argidx, const StringName &p_name) {
    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!custom_signals.contains(p_func));
    ERR_FAIL_INDEX(p_argidx, custom_signals[p_func].size());
    custom_signals[p_func][p_argidx].name = p_name;
}
StringView VisualScript::custom_signal_get_argument_name(const StringName &p_func, int p_argidx) const {

    ERR_FAIL_COND_V(!custom_signals.contains(p_func), nullptr);
    ERR_FAIL_INDEX_V(p_argidx, custom_signals.at(p_func).size(), nullptr);
    return custom_signals.at(p_func)[p_argidx].name;
}
void VisualScript::custom_signal_remove_argument(const StringName &p_func, int p_argidx) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!custom_signals.contains(p_func));
    ERR_FAIL_INDEX(p_argidx, custom_signals[p_func].size());
    custom_signals[p_func].erase_at(p_argidx);
}

int VisualScript::custom_signal_get_argument_count(const StringName &p_func) const {

    ERR_FAIL_COND_V(!custom_signals.contains(p_func), 0);
    return custom_signals.at(p_func).size();
}
void VisualScript::custom_signal_swap_argument(const StringName &p_func, int p_argidx, int p_with_argidx) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!custom_signals.contains(p_func));
    ERR_FAIL_INDEX(p_argidx, custom_signals[p_func].size());
    ERR_FAIL_INDEX(p_with_argidx, custom_signals[p_func].size());

    SWAP(custom_signals[p_func][p_argidx], custom_signals[p_func][p_with_argidx]);
}
void VisualScript::remove_custom_signal(const StringName &p_name) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!custom_signals.contains(p_name));
    custom_signals.erase(p_name);
}

void VisualScript::rename_custom_signal(const StringName &p_name, const StringName &p_new_name) {

    ERR_FAIL_COND(instances.size());
    ERR_FAIL_COND(!custom_signals.contains(p_name));
    if (p_new_name == p_name)
        return;

    ERR_FAIL_COND(!StringUtils::is_valid_identifier(p_new_name));

    ERR_FAIL_COND(functions.contains(p_new_name));
    ERR_FAIL_COND(variables.contains(p_new_name));
    ERR_FAIL_COND(custom_signals.contains(p_new_name));

    custom_signals[p_new_name] = custom_signals[p_name];
    custom_signals.erase(p_name);
}

void VisualScript::get_custom_signal_list(Vector<StringName> *r_custom_signals) const {

    for (const eastl::pair<const StringName,Vector<Argument> > &E : custom_signals) {
        r_custom_signals->push_back(E.first);
    }
    eastl::sort(r_custom_signals->begin(),r_custom_signals->end(),StringName::AlphCompare);
}

int VisualScript::get_available_id() const {

    int max_id = 0;
    for (const eastl::pair<const StringName,Function> &E : functions) {
        if (E.second.nodes.empty())
            continue;

        int last_id = E.second.nodes.rbegin()->first;
        max_id = M_MAX(max_id, last_id + 1);
    }

    return max_id;
}

/////////////////////////////////

bool VisualScript::can_instance() const {

    return true; //ScriptServer::is_scripting_enabled();
}

StringName VisualScript::get_instance_base_type() const {

    return base_type;
}

Ref<Script> VisualScript::get_base_script() const {
    return Ref<Script>(); // no inheritance in visual script
}

#ifdef TOOLS_ENABLED
void VisualScript::_placeholder_erased(PlaceHolderScriptInstance *p_placeholder) {

    placeholders.erase(p_placeholder);
}

void VisualScript::_update_placeholders() {

    if (placeholders.empty())
        return; //no bother if no placeholders
    Vector<PropertyInfo> pinfo;
    HashMap<StringName, Variant> values;

    for (eastl::pair<const StringName,Variable> &E : variables) {

        if (!E.second._export)
            continue;

        PropertyInfo p = E.second.info;
        p.name = E.first;
        pinfo.push_back(p);
        values[p.name] = E.second.default_value;
    }

    for (PlaceHolderScriptInstance *E : placeholders) {

        E->update(pinfo, values);
    }
}

#endif

ScriptInstance *VisualScript::instance_create(Object *p_this) {

#ifdef TOOLS_ENABLED

    if (!ScriptServer::is_scripting_enabled() && !is_tool_script) {

        PlaceHolderScriptInstance *sins = memnew(PlaceHolderScriptInstance(VisualScriptLanguage::singleton, Ref<Script>(this), p_this));
        placeholders.insert(sins);

        Vector<PropertyInfo> pinfo;
        HashMap<StringName, Variant> values;

        for (eastl::pair<const StringName,Variable> &E : variables) {

            if (!E.second._export)
                continue;

            PropertyInfo p = E.second.info;
            p.name = E.first;
            pinfo.push_back(p);
            values[p.name] = E.second.default_value;
        }

        sins->update(pinfo, values);

        return sins;
    }
#endif

    VisualScriptInstance *instance = memnew(VisualScriptInstance);
    instance->create(Ref<VisualScript>(this), p_this);

    if (VisualScriptLanguage::singleton->lock)
        VisualScriptLanguage::singleton->lock->lock();

    instances[p_this] = instance;

    if (VisualScriptLanguage::singleton->lock)
        VisualScriptLanguage::singleton->lock->unlock();

    return instance;
}

bool VisualScript::instance_has(Object *p_this) const {

    return instances.contains((Object *)p_this);
}

bool VisualScript::has_source_code() const {

    return false;
}

StringView VisualScript::get_source_code() const {

    return {};
}

void VisualScript::set_source_code(String p_code) {
}

Error VisualScript::reload(bool p_keep_state) {

    return OK;
}

bool VisualScript::is_tool() const {

    return is_tool_script;
}

bool VisualScript::is_valid() const {
    return true; //always valid
}

ScriptLanguage *VisualScript::get_language() const {

    return VisualScriptLanguage::singleton;
}

bool VisualScript::has_script_signal(const StringName &p_signal) const {

    return custom_signals.contains(p_signal);
}

void VisualScript::get_script_signal_list(Vector<MethodInfo> *r_signals) const {

    for (const eastl::pair<const StringName,Vector<Argument> > &E : custom_signals) {

        MethodInfo mi;
        mi.name = E.first;
        for (int i = 0; i < E.second.size(); i++) {
            PropertyInfo arg;
            arg.type = E.second[i].type;
            arg.name = E.second[i].name;
            mi.arguments.push_back(arg);
        }

        r_signals->push_back(mi);
    }
}

bool VisualScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {

    if (!variables.contains(p_property))
        return false;

    r_value = variables.at(p_property).default_value;
    return true;
}
void VisualScript::get_script_method_list(Vector<MethodInfo> *p_list) const {

    for (const eastl::pair<const StringName,Function> &E : functions) {

        MethodInfo mi;
        mi.name = E.first;
        if (E.second.function_id >= 0) {

            Ref<VisualScriptFunction> func = dynamic_ref_cast<VisualScriptFunction>(E.second.nodes.at(E.second.function_id).node);
            if (func) {

                for (int i = 0; i < func->get_argument_count(); i++) {
                    PropertyInfo arg;
                    arg.name = func->get_argument_name(i);
                    arg.type = func->get_argument_type(i);
                    mi.arguments.push_back(arg);
                }
                p_list->push_back(mi);
            }
        }

    }
}

bool VisualScript::has_method(const StringName &p_method) const {

    return functions.contains(p_method);
}
MethodInfo VisualScript::get_method_info(const StringName &p_method) const {

    const HashMap<StringName, Function>::const_iterator E = functions.find(p_method);
    if (E==functions.end())
        return MethodInfo();

    MethodInfo mi;
    mi.name = E->first;
    if (E->second.function_id >= 0) {

        Ref<VisualScriptFunction> func = dynamic_ref_cast<VisualScriptFunction>(E->second.nodes.at(E->second.function_id).node);
        if (func) {

            for (int i = 0; i < func->get_argument_count(); i++) {
                PropertyInfo arg;
                arg.name = func->get_argument_name(i);
                arg.type = func->get_argument_type(i);
                mi.arguments.push_back(arg);
            }

            if (!func->is_sequenced()) {
                mi.flags |= METHOD_FLAG_CONST;
            }
        }
    }

    return mi;
}

void VisualScript::get_script_property_list(Vector<PropertyInfo> *p_list) const {

    Vector<StringName> vars;
    get_variable_list(&vars);

    for (int i=0,fin=vars.size(); i<fin; ++i) {
        //if (!variables[E.second]._export)
        //	continue;
        PropertyInfo pi = variables.at(vars[i]).info;
        pi.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;
        p_list->push_back(pi);
    }
}

int VisualScript::get_member_line(const StringName &p_member) const {
#ifdef TOOLS_ENABLED
    if (has_function(p_member)) {
        for (const auto &E : functions.at(p_member).nodes) {
            if (object_cast<VisualScriptFunction>(E.second.node.get()))
                return E.first;
        }
    }
#endif
    return -1;
}

#ifdef TOOLS_ENABLED
bool VisualScript::are_subnodes_edited() const {

    for (const eastl::pair<const StringName,Function> &E : functions) {

        for (const auto &F : E.second.nodes) {
            if (F.second.node->get_tooling_interface()->is_edited()) {
                return true;
            }
        }
    }

    return false;
}
#endif

void VisualScript::_set_data(const Dictionary &p_data) {

    Dictionary d = p_data;
    if (d.has("base_type"))
        base_type = d["base_type"].as<StringName>();

    variables.clear();
    Array vars = d["variables"].as<Array>();
    for (int i = 0; i < vars.size(); i++) {

        Dictionary v = vars[i].as<Dictionary>();
        StringName name = v["name"].as<StringName>();
        add_variable(name);
        _set_variable_info(name, v);
        set_variable_default_value(name, v["default_value"]);
        set_variable_export(name, v.has("export") && v["export"].as<bool>());
    }

    custom_signals.clear();
    Array sigs = d["signals"].as<Array>();
    for (int i = 0; i < sigs.size(); i++) {

        Dictionary cs = sigs[i].as<Dictionary>();
        add_custom_signal(cs["name"].as<StringName>());

        Array args = cs["arguments"].as<Array>();
        for (int j = 0; j < args.size(); j += 2) {
            custom_signal_add_argument(cs["name"].as<StringName>(), args[j + 1].as<VariantType>(), args[j].as<StringName>());
        }
    }

    Array funcs = d["functions"].as<Array>();
    functions.clear();

    Vector2 last_pos = Vector2(-100 * funcs.size(), -100 * funcs.size()); // this is the center of the last fn box
    Vector2 last_size = Vector2(0.0, 0.0);

    for (int i = 0; i < funcs.size(); i++) {

        Dictionary func = funcs[i].as<Dictionary>();

        StringName name = func["name"].as<StringName>();
        //int id=func["function_id"];
        add_function(name);

        set_function_scroll(name, func["scroll"].as<Vector2>());

        Array nodes = func["nodes"].as<Array>();
        if (!d.has("vs_unify") && nodes.size() > 0) {
            Vector2 top_left = nodes[1].as<Vector2>();
            Vector2 bottom_right = nodes[1].as<Vector2>();

        for (int j = 0; j < nodes.size(); j += 3) {
                Point2 pos = nodes[j + 1].as<Vector2>();
                if (pos.y > top_left.y) {
                    top_left.y = pos.y;
                }
                if (pos.y < bottom_right.y) {
                    bottom_right.y = pos.y;
                }
                if (pos.x > bottom_right.x) {
                    bottom_right.x = pos.x;
                }
                if (pos.x < top_left.x) {
                    top_left.x = pos.x;
                }
            }

            Vector2 size = Vector2(bottom_right.x - top_left.x, top_left.y - bottom_right.y);

            Vector2 offset = last_pos + (last_size / 2.0) + (size / 2.0); // dunno I might just keep it in one axis but diagonal feels better....

            last_pos = offset;
            last_size = size;

            for (int j = 0; j < nodes.size(); j += 3) {
                add_node(name, nodes[j].as<int>(), refFromVariant<VisualScriptNode>(nodes[j + 2]), offset + nodes[j + 1].as<Vector2>()); // also add an additional buffer if you want to
        }

        } else {
            for (int j = 0; j < nodes.size(); j += 3) {
                add_node(name, nodes[j].as<int>(), refFromVariant<VisualScriptNode>(nodes[j + 2]), nodes[j + 1].as<Vector2>());
            }
        }
        Array sequence_connections = func["sequence_connections"].as<Array>();

        for (int j = 0; j < sequence_connections.size(); j += 3) {

            sequence_connect(name, sequence_connections[j + 0].as<int>(), sequence_connections[j + 1].as<int>(), sequence_connections[j + 2].as<int>());
        }

        Array data_connections = func["data_connections"].as<Array>();

        for (int j = 0; j < data_connections.size(); j += 4) {

            data_connect(name, data_connections[j + 0].as<int>(), data_connections[j + 1].as<int>(), data_connections[j + 2].as<int>(), data_connections[j + 3].as<int>());
        }
    }

    if (d.has("is_tool_script"))
        is_tool_script = d["is_tool_script"].as<bool>();
    else
        is_tool_script = false;
}

Dictionary VisualScript::_get_data() const {

    Dictionary d;
    d["base_type"] = base_type;
    Array vars;
    for (const eastl::pair<const StringName,Variable> &E : variables) {

        Dictionary var = _get_variable_info(E.first);
        var["name"] = E.first; //make sure it's the right one
        var["default_value"] = E.second.default_value;
        var["export"] = E.second._export;
        vars.push_back(var);
    }
    d["variables"] = vars;

    Array sigs;
    for (const eastl::pair<const StringName,Vector<Argument> > &E : custom_signals) {

        Dictionary cs;
        cs["name"] = E.first;
        Array args;
        for (int i = 0; i < E.second.size(); i++) {
            args.push_back(E.second[i].name);
            args.push_back(int(E.second[i].type));
        }
        cs["arguments"] = args;

        sigs.push_back(cs);
    }

    d["signals"] = sigs;

    Array funcs;

    for (const eastl::pair<const StringName,Function> &E : functions) {

        Dictionary func;
        func["name"] = E.first;
        func["function_id"] = E.second.function_id;
        func["scroll"] = E.second.scroll;

        Array nodes;

        for (const auto &F : E.second.nodes) {

            nodes.push_back(F.first);
            nodes.push_back(F.second.pos);
            nodes.push_back(F.second.node);
        }

        func["nodes"] = nodes;

        Array sequence_connections;

        for (const SequenceConnection &F : E.second.sequence_connections) {

            sequence_connections.push_back(F.from_node);
            sequence_connections.push_back(F.from_output);
            sequence_connections.push_back(F.to_node);
        }

        func["sequence_connections"] = sequence_connections;

        Array data_connections;

        for (const DataConnection &F : E.second.data_connections) {

            data_connections.push_back(F.from_node);
            data_connections.push_back(F.from_port);
            data_connections.push_back(F.to_node);
            data_connections.push_back(F.to_port);
        }

        func["data_connections"] = data_connections;

        funcs.push_back(func);
    }

    d["functions"] = funcs;

    d["is_tool_script"] = is_tool_script;
    d["vs_unify"] = true;

    return d;
}

void VisualScript::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("_node_ports_changed"), &VisualScript::_node_ports_changed);

    MethodBinder::bind_method(D_METHOD("add_function", {"name"}), &VisualScript::add_function);
    MethodBinder::bind_method(D_METHOD("has_function", {"name"}), &VisualScript::has_function);
    MethodBinder::bind_method(D_METHOD("remove_function", {"name"}), &VisualScript::remove_function);
    MethodBinder::bind_method(D_METHOD("rename_function", {"name", "new_name"}), &VisualScript::rename_function);
    MethodBinder::bind_method(D_METHOD("set_function_scroll", {"name", "ofs"}), &VisualScript::set_function_scroll);
    MethodBinder::bind_method(D_METHOD("get_function_scroll", {"name"}), &VisualScript::get_function_scroll);

    MethodBinder::bind_method(D_METHOD("add_node", {"func", "id", "node", "position"}), &VisualScript::add_node, {DEFVAL(Point2())});
    MethodBinder::bind_method(D_METHOD("remove_node", {"func", "id"}), &VisualScript::remove_node);
    MethodBinder::bind_method(D_METHOD("get_function_node_id", {"name"}), &VisualScript::get_function_node_id);

    MethodBinder::bind_method(D_METHOD("get_node", {"func", "id"}), &VisualScript::get_node);
    MethodBinder::bind_method(D_METHOD("has_node", {"func", "id"}), &VisualScript::has_node);
    MethodBinder::bind_method(D_METHOD("set_node_position", {"func", "id", "position"}), &VisualScript::set_node_position);
    MethodBinder::bind_method(D_METHOD("get_node_position", {"func", "id"}), &VisualScript::get_node_position);

    MethodBinder::bind_method(D_METHOD("sequence_connect", {"func", "from_node", "from_output", "to_node"}), &VisualScript::sequence_connect);
    MethodBinder::bind_method(D_METHOD("sequence_disconnect", {"func", "from_node", "from_output", "to_node"}), &VisualScript::sequence_disconnect);
    MethodBinder::bind_method(D_METHOD("has_sequence_connection", {"func", "from_node", "from_output", "to_node"}), &VisualScript::has_sequence_connection);

    MethodBinder::bind_method(D_METHOD("data_connect", {"func", "from_node", "from_port", "to_node", "to_port"}), &VisualScript::data_connect);
    MethodBinder::bind_method(D_METHOD("data_disconnect", {"func", "from_node", "from_port", "to_node", "to_port"}), &VisualScript::data_disconnect);
    MethodBinder::bind_method(D_METHOD("has_data_connection", {"func", "from_node", "from_port", "to_node", "to_port"}), &VisualScript::has_data_connection);

    MethodBinder::bind_method(D_METHOD("add_variable", {"name", "default_value", "export"}), &VisualScript::add_variable, {DEFVAL(Variant()), DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("has_variable", {"name"}), &VisualScript::has_variable);
    MethodBinder::bind_method(D_METHOD("remove_variable", {"name"}), &VisualScript::remove_variable);
    MethodBinder::bind_method(D_METHOD("set_variable_default_value", {"name", "value"}), &VisualScript::set_variable_default_value);
    MethodBinder::bind_method(D_METHOD("get_variable_default_value", {"name"}), &VisualScript::get_variable_default_value);
    MethodBinder::bind_method(D_METHOD("set_variable_info", {"name", "value"}), &VisualScript::_set_variable_info);
    MethodBinder::bind_method(D_METHOD("get_variable_info", {"name"}), &VisualScript::_get_variable_info);
    MethodBinder::bind_method(D_METHOD("set_variable_export", {"name", "enable"}), &VisualScript::set_variable_export);
    MethodBinder::bind_method(D_METHOD("get_variable_export", {"name"}), &VisualScript::get_variable_export);
    MethodBinder::bind_method(D_METHOD("rename_variable", {"name", "new_name"}), &VisualScript::rename_variable);

    MethodBinder::bind_method(D_METHOD("add_custom_signal", {"name"}), &VisualScript::add_custom_signal);
    MethodBinder::bind_method(D_METHOD("has_custom_signal", {"name"}), &VisualScript::has_custom_signal);
    MethodBinder::bind_method(D_METHOD("custom_signal_add_argument", {"name", "type", "argname", "index"}), &VisualScript::custom_signal_add_argument, {DEFVAL(-1)});
    MethodBinder::bind_method(D_METHOD("custom_signal_set_argument_type", {"name", "argidx", "type"}), &VisualScript::custom_signal_set_argument_type);
    MethodBinder::bind_method(D_METHOD("custom_signal_get_argument_type", {"name", "argidx"}), &VisualScript::custom_signal_get_argument_type);
    MethodBinder::bind_method(D_METHOD("custom_signal_set_argument_name", {"name", "argidx", "argname"}), &VisualScript::custom_signal_set_argument_name);
    MethodBinder::bind_method(D_METHOD("custom_signal_get_argument_name", {"name", "argidx"}), &VisualScript::custom_signal_get_argument_name);
    MethodBinder::bind_method(D_METHOD("custom_signal_remove_argument", {"name", "argidx"}), &VisualScript::custom_signal_remove_argument);
    MethodBinder::bind_method(D_METHOD("custom_signal_get_argument_count", {"name"}), &VisualScript::custom_signal_get_argument_count);
    MethodBinder::bind_method(D_METHOD("custom_signal_swap_argument", {"name", "argidx", "withidx"}), &VisualScript::custom_signal_swap_argument);
    MethodBinder::bind_method(D_METHOD("remove_custom_signal", {"name"}), &VisualScript::remove_custom_signal);
    MethodBinder::bind_method(D_METHOD("rename_custom_signal", {"name", "new_name"}), &VisualScript::rename_custom_signal);

    //MethodBinder::bind_method(D_METHOD("set_variable_info","name","info"),&VScript::set_variable_info);
    //MethodBinder::bind_method(D_METHOD("get_variable_info","name"),&VScript::set_variable_info);

    MethodBinder::bind_method(D_METHOD("set_instance_base_type", {"type"}), &VisualScript::set_instance_base_type);

    MethodBinder::bind_method(D_METHOD("_set_data", {"data"}), &VisualScript::_set_data);
    MethodBinder::bind_method(D_METHOD("_get_data"), &VisualScript::_get_data);

    ADD_PROPERTY(PropertyInfo(VariantType::DICTIONARY, "data", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_data", "_get_data");

    ADD_SIGNAL(MethodInfo("node_ports_changed", PropertyInfo(VariantType::STRING, "function"), PropertyInfo(VariantType::INT, "id")));
}

VisualScript::VisualScript() {

    base_type = "Object";
    is_tool_script = false;
}

StringName VisualScript::get_default_func() const {
    return StringName("f_312843592");
}

Set<int> VisualScript::get_output_sequence_ports_connected(StringView edited_func, int from_node) {
    Vector<VisualScript::SequenceConnection> sc;
    get_sequence_connection_list(StringName(edited_func), &sc);
    Set<int> connected;
    for (const auto & E : sc) {
        if (E.from_node == from_node) {
            connected.insert(int(E.from_output));
        }
    }
    return connected;
}

VisualScript::~VisualScript() {

    while (!functions.empty()) {
        remove_function(functions.begin()->first);
    }
}

////////////////////////////////////////////

bool VisualScriptInstance::set(const StringName &p_name, const Variant &p_value) {

    HashMap<StringName, Variant>::iterator E = variables.find(p_name);
    if (E==variables.end())
        return false;

    E->second = p_value;

    return true;
}

bool VisualScriptInstance::get(const StringName &p_name, Variant &r_ret) const {

    const HashMap<StringName, Variant>::const_iterator E = variables.find(p_name);
    if (E==variables.end())
        return false;

    r_ret = E->second;
    return true;
}
void VisualScriptInstance::get_property_list(Vector<PropertyInfo> *p_properties) const {

    for (const eastl::pair<const StringName,VisualScript::Variable> &E : script->variables) {

        if (!E.second._export)
            continue;
        PropertyInfo p = E.second.info;
        p.name = E.first;
        p.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;
        p_properties->push_back(p);
    }
}
VariantType VisualScriptInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {

    const HashMap<StringName, VisualScript::Variable>::const_iterator E = script->variables.find(p_name);
    if (E==script->variables.end()) {
        if (r_is_valid)
            *r_is_valid = false;
        ERR_FAIL_V(VariantType::NIL);
    }

    if (r_is_valid)
        *r_is_valid = true;

    return E->second.info.type;
}

void VisualScriptInstance::get_method_list(Vector<MethodInfo> *p_list) const {

    for (const eastl::pair<const StringName,VisualScript::Function> &E : script->functions) {

        if (E.first == script->get_default_func()) {
            continue;
        }
        MethodInfo mi;
        mi.name = E.first;
        if (E.second.function_id >= 0 && E.second.nodes.contains(E.second.function_id)) {

            Ref<VisualScriptFunction> vsf = dynamic_ref_cast<VisualScriptFunction>(E.second.nodes.at(E.second.function_id).node);
            if (vsf) {

                for (int i = 0; i < vsf->get_argument_count(); i++) {
                    PropertyInfo arg;
                    arg.name = vsf->get_argument_name(i);
                    arg.type = vsf->get_argument_type(i);

                    mi.arguments.push_back(arg);
                }

                if (!vsf->is_sequenced()) { //assumed constant if not sequenced
                    mi.flags |= METHOD_FLAG_CONST;
                }

            }
        }

        p_list->push_back(mi);
    }
}
bool VisualScriptInstance::has_method(const StringName &p_method) const {
    if (p_method == script->get_default_func())
        return false;

    return script->functions.contains(p_method);
}

//#define VSDEBUG(m_text) print_line(m_text)
#define VSDEBUG(m_text)

void VisualScriptInstance::_dependency_step(VisualScriptNodeInstance *node, int p_pass, int *pass_stack, const Variant **input_args, Variant **output_args, Variant *variant_stack, Callable::CallError &r_error, String &error_str, VisualScriptNodeInstance **r_error_node) {

    ERR_FAIL_COND(node->pass_idx == -1);

    if (pass_stack[node->pass_idx] == p_pass)
        return;

    pass_stack[node->pass_idx] = p_pass;

    if (!node->dependencies.empty()) {

        int dc = node->dependencies.size();
        VisualScriptNodeInstance **deps = node->dependencies.data();

        for (int i = 0; i < dc; i++) {

            _dependency_step(deps[i], p_pass, pass_stack, input_args, output_args, variant_stack, r_error, error_str, r_error_node);
            if (r_error.error != Callable::CallError::CALL_OK)
                return;
        }
    }

    for (int i = 0; i < node->input_port_count; i++) {

        int index = node->input_ports[i] & VisualScriptNodeInstance::INPUT_MASK;

        if (node->input_ports[i] & VisualScriptNodeInstance::INPUT_DEFAULT_VALUE_BIT) {
            //is a default value (unassigned input port)
            input_args[i] = &default_values[index];
        } else {
            //regular temporary in stack
            input_args[i] = &variant_stack[index];
        }
    }
    for (int i = 0; i < node->output_port_count; i++) {
        output_args[i] = &variant_stack[node->output_ports[i]];
    }

    Variant *working_mem = node->working_mem_idx >= 0 ? &variant_stack[node->working_mem_idx] : (Variant *)nullptr;

    node->step(input_args, output_args, VisualScriptNodeInstance::START_MODE_BEGIN_SEQUENCE, working_mem, r_error, error_str);
    //ignore return
    if (r_error.error != Callable::CallError::CALL_OK) {
        *r_error_node = node;
    }
}

Variant VisualScriptInstance::_call_internal(const StringName &p_method, void *p_stack, int p_stack_size, VisualScriptNodeInstance *p_node, int p_flow_stack_pos, int p_pass, bool p_resuming_yield, Callable::CallError &r_error) {

    Map<StringName, Function>::iterator F = functions.find(p_method);
    ERR_FAIL_COND_V(F==functions.end(), Variant());
    Function *f = &F->second;

    //this call goes separate, so it can e yielded and suspended
    Variant *variant_stack = (Variant *)p_stack;
    bool *sequence_bits = (bool *)(variant_stack + f->max_stack);
    const Variant **input_args = (const Variant **)(sequence_bits + f->node_count);
    Variant **output_args = (Variant **)(input_args + max_input_args);
    int flow_max = f->flow_stack_size;
    int *flow_stack = flow_max ? (int *)(output_args + max_output_args) : (int *)nullptr;
    int *pass_stack = flow_stack ? (int *)(flow_stack + flow_max) : (int *)nullptr;

    String error_str;

    VisualScriptNodeInstance *node = p_node;
    bool error = false;
    int current_node_id = f->node;
    Variant return_value;
    Variant *working_mem = nullptr;

    int flow_stack_pos = p_flow_stack_pos;

#ifdef DEBUG_ENABLED
    if (ScriptDebugger::get_singleton()) {
        VisualScriptLanguage::singleton->enter_function(this, &p_method, variant_stack, &working_mem, &current_node_id);
    }
#endif

    while (true) {

        p_pass++; //increment pass
        current_node_id = node->get_id();

        VSDEBUG("==========AT NODE: " + itos(current_node_id) + " base: " + node->get_base_node()->get_class_name());
        VSDEBUG("AT STACK POS: " + itos(flow_stack_pos));

        //setup working mem
        working_mem = node->working_mem_idx >= 0 ? &variant_stack[node->working_mem_idx] : (Variant *)nullptr;

        VSDEBUG("WORKING MEM: " + itos(node->working_mem_idx));

        if (current_node_id == f->node) {
            //if function node, set up function arguments from beginning of stack

            for (int i = 0; i < f->argument_count; i++) {
                input_args[i] = &variant_stack[i];
            }
        } else {

            //run dependencies first

            if (!node->dependencies.empty()) {

                int dc = node->dependencies.size();
                VisualScriptNodeInstance **deps = node->dependencies.data();

                for (int i = 0; i < dc; i++) {

                    _dependency_step(deps[i], p_pass, pass_stack, input_args, output_args, variant_stack, r_error, error_str, &node);
                    if (r_error.error != Callable::CallError::CALL_OK) {
                        error = true;
                        current_node_id = node->id;
                        break;
                    }
                }
            }

            if (!error) {

                //setup input pointers normally
                VSDEBUG("INPUT PORTS: " + itos(node->input_port_count));

                for (int i = 0; i < node->input_port_count; i++) {

                    int index = node->input_ports[i] & VisualScriptNodeInstance::INPUT_MASK;

                    if (node->input_ports[i] & VisualScriptNodeInstance::INPUT_DEFAULT_VALUE_BIT) {
                        //is a default value (unassigned input port)
                        input_args[i] = &default_values[index];
                        VSDEBUG("\tPORT " + itos(i) + " DEFAULT VAL");
                    } else {
                        //regular temporary in stack
                        input_args[i] = &variant_stack[index];
                        VSDEBUG("PORT " + itos(i) + " AT STACK " + itos(index));
                    }
                }
            }
        }

        if (error)
            break;

        //setup output pointers

        VSDEBUG("OUTPUT PORTS: " + itos(node->output_port_count));
        for (int i = 0; i < node->output_port_count; i++) {
            output_args[i] = &variant_stack[node->output_ports[i]];
            VSDEBUG("PORT " + itos(i) + " AT STACK " + itos(node->output_ports[i]));
        }

        //do step

        VisualScriptNodeInstance::StartMode start_mode;
        {
            if (p_resuming_yield) {
                start_mode = VisualScriptNodeInstance::START_MODE_RESUME_YIELD;
                p_resuming_yield = false; // should resume only the first time
            } else if (flow_stack && (flow_stack[flow_stack_pos] & VisualScriptNodeInstance::FLOW_STACK_PUSHED_BIT)) {
                //if there is a push bit, it means we are continuing a sequence
                start_mode = VisualScriptNodeInstance::START_MODE_CONTINUE_SEQUENCE;
            } else {
                start_mode = VisualScriptNodeInstance::START_MODE_BEGIN_SEQUENCE;
            }
        }

        VSDEBUG("STEP - STARTSEQ: " + itos(start_mode));

        int ret = node->step(input_args, output_args, start_mode, working_mem, r_error, error_str);

        if (r_error.error != Callable::CallError::CALL_OK) {
            //use error from step
            error = true;
            break;
        }

        if (ret & VisualScriptNodeInstance::STEP_YIELD_BIT) {
            //yielded!
            if (node->get_working_memory_size() == 0) {
                r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
                error_str = RTR_utf8("A node yielded without working memory, please read the docs on how to yield properly!");
                error = true;
                break;

            } else {
                Ref<VisualScriptFunctionState> state = refFromVariant<VisualScriptFunctionState>(*working_mem);
                if (not state) {

                    r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
                    error_str = RTR_utf8("Node yielded, but did not return a function state in the first working memory.");
                    error = true;
                    break;
                }

                //step 1, capture all state
                state->instance_id = get_owner_ptr()->get_instance_id();
                state->script_id = get_script()->get_instance_id();
                state->instance = this;
                state->function = p_method;
                state->working_mem_index = node->working_mem_idx;
                state->variant_stack_size = f->max_stack;
                state->node = node;
                state->flow_stack_pos = flow_stack_pos;
                state->stack.resize(p_stack_size);
                state->pass = p_pass;
                memcpy(state->stack.data(), p_stack, p_stack_size);
                //step 2, run away, return directly
                r_error.error = Callable::CallError::CALL_OK;

#ifdef DEBUG_ENABLED
                //will re-enter later, so exiting
                if (ScriptDebugger::get_singleton()) {
                    VisualScriptLanguage::singleton->exit_function();
                }
#endif

                return state;
            }
        }

#ifdef DEBUG_ENABLED
        if (ScriptDebugger::get_singleton()) {
            // line
            bool do_break = false;

            if (ScriptDebugger::get_singleton()->get_lines_left() > 0) {

                if (ScriptDebugger::get_singleton()->get_depth() <= 0)
                    ScriptDebugger::get_singleton()->set_lines_left(ScriptDebugger::get_singleton()->get_lines_left() - 1);
                if (ScriptDebugger::get_singleton()->get_lines_left() <= 0)
                    do_break = true;
            }

            if (ScriptDebugger::get_singleton()->is_breakpoint(current_node_id, source))
                do_break = true;

            if (do_break) {
                VisualScriptLanguage::singleton->debug_break(("Breakpoint"), true);
            }

            ScriptDebugger::get_singleton()->line_poll();
        }
#endif
        int output = ret & VisualScriptNodeInstance::STEP_MASK;

        VSDEBUG("STEP RETURN: " + itos(ret));

        if (ret & VisualScriptNodeInstance::STEP_EXIT_FUNCTION_BIT) {
            if (node->get_working_memory_size() == 0) {

                r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
                error_str = RTR_utf8("Return value must be assigned to first element of node working memory! Fix your node please.");
                error = true;
            } else {
                //assign from working memory, first element
                return_value = *working_mem;
            }

            VSDEBUG("EXITING FUNCTION - VALUE " + String(return_value));
            break; //exit function requested, bye
        }

        VisualScriptNodeInstance *next = nullptr; //next node

        if ((ret == output || ret & VisualScriptNodeInstance::STEP_FLAG_PUSH_STACK_BIT) && node->sequence_output_count) {
            //if no exit bit was set, and has sequence outputs, guess next node
            if (output >= node->sequence_output_count) {
                r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
                error_str = RTR_utf8("Node returned an invalid sequence output: ") + itos(output);
                error = true;
                break;
            }

            next = node->sequence_outputs[output];
            if (next) {
                VSDEBUG("GOT NEXT NODE - " + itos(next->get_id()));
            } else {
                VSDEBUG("GOT NEXT NODE - NULL");
            }
        }

        if (flow_stack) {

            //update flow stack pos (may have changed)
            flow_stack[flow_stack_pos] = current_node_id;

            //add stack push bit if requested
            if (ret & VisualScriptNodeInstance::STEP_FLAG_PUSH_STACK_BIT) {

                flow_stack[flow_stack_pos] |= VisualScriptNodeInstance::FLOW_STACK_PUSHED_BIT;
                sequence_bits[node->sequence_index] = true; //remember sequence bit
                VSDEBUG("NEXT SEQ - FLAG BIT");
            } else {
                sequence_bits[node->sequence_index] = false; //forget sequence bit
                VSDEBUG("NEXT SEQ - NORMAL");
            }

            if (ret & VisualScriptNodeInstance::STEP_FLAG_GO_BACK_BIT) {
                //go back request

                if (flow_stack_pos > 0) {
                    flow_stack_pos--;
                    node = instances[flow_stack[flow_stack_pos] & VisualScriptNodeInstance::FLOW_STACK_MASK];
                    VSDEBUG("NEXT IS GO BACK");
                } else {
                    VSDEBUG("NEXT IS GO BACK, BUT NO NEXT SO EXIT");
                    break; //simply exit without value or error
                }
            } else if (next) {

                if (sequence_bits[next->sequence_index]) {
                    // what happened here is that we are entering a node that is in the middle of doing a sequence (pushed stack) from the front
                    // because each node has a working memory, we can't really do a sub-sequence
                    // as a result, the sequence will be restarted and the stack will roll back to find where this node
                    // started the sequence

                    bool found = false;

                    for (int i = flow_stack_pos; i >= 0; i--) {

                        if ((flow_stack[i] & VisualScriptNodeInstance::FLOW_STACK_MASK) == next->get_id()) {
                            flow_stack_pos = i; //roll back and remove bit
                            flow_stack[i] = next->get_id();
                            sequence_bits[next->sequence_index] = false;
                            found = true;
                        }
                    }

                    if (!found) {
                        r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
                        error_str = RTR_utf8("Found sequence bit but not the node in the stack, report bug!");
                        error = true;
                        break;
                    }

                    node = next;
                    VSDEBUG("RE-ENTERED A LOOP, RETURNED STACK POS TO - " + itos(flow_stack_pos));

                } else {
                    // check for stack overflow
                    if (flow_stack_pos + 1 >= flow_max) {
                        r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
                        error_str = RTR_utf8("Stack overflow with stack depth: ") + itos(output);
                        error = true;
                        break;
                    }

                    node = next;

                    flow_stack_pos++;
                    flow_stack[flow_stack_pos] = node->get_id();

                    VSDEBUG("INCREASE FLOW STACK");
                }

            } else {
                //no next node, try to go back in stack to pushed bit

                bool found = false;

                for (int i = flow_stack_pos; i >= 0; i--) {

                    VSDEBUG("FS " + itos(i) + " - " + itos(flow_stack[i]));
                    if (flow_stack[i] & VisualScriptNodeInstance::FLOW_STACK_PUSHED_BIT) {

                        node = instances[flow_stack[i] & VisualScriptNodeInstance::FLOW_STACK_MASK];
                        flow_stack_pos = i;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    VSDEBUG("NO NEXT NODE, NO GO BACK, EXITING");
                    break; //done, couldn't find a push stack bit
                }

                VSDEBUG("NO NEXT NODE, GO BACK TO: " + itos(flow_stack_pos));
            }
        } else {

            node = next; //stackless mode, simply assign next node
        }
    }

    if (error) {

        //error
        // function, file, line, error, explanation
        String err_file = script->get_path();
        String err_func = p_method.asCString();
        int err_line = current_node_id; //not a line but it works as one

        if (node && (r_error.error != Callable::CallError::CALL_ERROR_INVALID_METHOD || error_str.empty())) {

            if (!error_str.empty()) {
                error_str += ' ';
            }

            if (r_error.error == Callable::CallError::CALL_ERROR_INVALID_ARGUMENT) {
                int errorarg = r_error.argument;
                error_str += "Cannot convert argument " + itos(errorarg + 1) + " to " + Variant::get_type_name(r_error.expected) + ".";
            } else if (r_error.error == Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS) {
                error_str += "Expected " + itos(r_error.argument) + " arguments.";
            } else if (r_error.error == Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS) {
                error_str += "Expected " + itos(r_error.argument) + " arguments.";
            } else if (r_error.error == Callable::CallError::CALL_ERROR_INVALID_METHOD) {
                error_str += ("Invalid Call.");
            } else if (r_error.error == Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL) {
                error_str += ("Base Instance is null");
            }
        }

        //if (!GDScriptLanguage::get_singleton()->debug_break(err_text,false)) {
        // debugger break did not happen

        if (!VisualScriptLanguage::singleton->debug_break(error_str, false)) {

            _err_print_error(err_func.c_str(), err_file.c_str(), err_line, error_str,{ }, ERR_HANDLER_SCRIPT);
        }

        //}
    } else {

        //return_value=
    }

#ifdef DEBUG_ENABLED
    if (ScriptDebugger::get_singleton()) {
        VisualScriptLanguage::singleton->exit_function();
    }
#endif

    //clean up variant stack
    for (int i = 0; i < f->max_stack; i++) {
        variant_stack[i].~Variant();
    }

    return return_value;
}

Variant VisualScriptInstance::call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    r_error.error = Callable::CallError::CALL_OK; //ok by default

    Map<StringName, Function>::iterator F = functions.find(p_method);
    if (F==functions.end()) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
        return Variant();
    }

    VSDEBUG("CALLING: " + String(p_method));

    Function *f = &F->second;

    int total_stack_size = 0;

    total_stack_size += f->max_stack * sizeof(Variant); //variants
    total_stack_size += f->node_count * sizeof(bool);
    total_stack_size += (max_input_args + max_output_args) * sizeof(Variant *); //arguments
    total_stack_size += f->flow_stack_size * sizeof(int); //flow
    total_stack_size += f->pass_stack_size * sizeof(int);

    VSDEBUG("STACK SIZE: " + itos(total_stack_size));
    VSDEBUG("STACK VARIANTS: : " + itos(f->max_stack));
    VSDEBUG("SEQBITS: : " + itos(f->node_count));
    VSDEBUG("MAX INPUT: " + itos(max_input_args));
    VSDEBUG("MAX OUTPUT: " + itos(max_output_args));
    VSDEBUG("FLOW STACK SIZE: " + itos(f->flow_stack_size));
    VSDEBUG("PASS STACK SIZE: " + itos(f->pass_stack_size));

    void *stack = alloca(total_stack_size);

    Variant *variant_stack = (Variant *)stack;
    bool *sequence_bits = (bool *)(variant_stack + f->max_stack);
    const Variant **input_args = (const Variant **)(sequence_bits + f->node_count);
    Variant **output_args = (Variant **)(input_args + max_input_args);
    int flow_max = f->flow_stack_size;
    int *flow_stack = flow_max ? (int *)(output_args + max_output_args) : (int *)nullptr;
    //int *pass_stack = flow_stack ? (int *)(flow_stack + flow_max) : (int *)nullptr;

    for (int i = 0; i < f->node_count; i++) {
        sequence_bits[i] = false; //all starts as false
    }

    //memset(pass_stack,0, f->pass_stack_size * sizeof(int));

    Map<int, VisualScriptNodeInstance *>::iterator E = instances.find(f->node);
    if (E==instances.end()) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;

        ERR_FAIL_V_MSG(Variant(), "No VisualScriptFunction node in function.");
    }

    VisualScriptNodeInstance *node = E->second;

    if (flow_stack) {
        flow_stack[0] = node->get_id();
    }

    VSDEBUG("ARGUMENTS: " + itos(f->argument_count) = " RECEIVED: " + itos(p_argcount));

    if (p_argcount < f->argument_count) {
        r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
        r_error.argument = node->get_input_port_count();

        return Variant();
    }

    if (p_argcount > f->argument_count) {
        r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
        r_error.argument = node->get_input_port_count();

        return Variant();
    }

    //allocate variant stack
    for (int i = 0; i < f->max_stack; i++) {
        memnew_placement(&variant_stack[i], Variant);
    }

    //allocate function arguments (must be copied for yield to work properly)
    for (int i = 0; i < p_argcount; i++) {
        variant_stack[i] = *p_args[i];
    }

    return _call_internal(p_method, stack, total_stack_size, node, 0, 0, false, r_error);
}

void VisualScriptInstance::notification(int p_notification) {

    //do nothing as this is called using virtual

    Variant what = p_notification;
    const Variant *whatp = &what;
    Callable::CallError ce;
    call(VisualScriptLanguage::singleton->notification, &whatp, 1, ce); //do as call
}

String VisualScriptInstance::to_string(bool *r_valid) {
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

Ref<Script> VisualScriptInstance::get_script() const {

    return script;
}

MultiplayerAPI_RPCMode VisualScriptInstance::get_rpc_mode(const StringName &p_method) const {
    if (p_method == script->get_default_func())
        return MultiplayerAPI_RPCMode(0);

    const HashMap<StringName, VisualScript::Function>::const_iterator E = script->functions.find(p_method);
    if (E==script->functions.end()) {
        return MultiplayerAPI_RPCMode(0);
    }

    if (E->second.function_id >= 0 && E->second.nodes.contains(E->second.function_id)) {

        Ref<VisualScriptFunction> vsf = dynamic_ref_cast<VisualScriptFunction>(E->second.nodes.at(E->second.function_id).node);
        if (vsf) {

            return vsf->get_rpc_mode();
        }
    }

    return MultiplayerAPI_RPCMode(0);
}

MultiplayerAPI_RPCMode VisualScriptInstance::get_rset_mode(const StringName &p_variable) const {

    return MultiplayerAPI_RPCMode(0);
}

void VisualScriptInstance::create(const Ref<VisualScript> &p_script, Object *p_owner) {

    script = p_script;
    owner = p_owner;
    source = StringName(p_script->get_path());

    max_input_args = 0;
    max_output_args = 0;

    if (object_cast<Node>(p_owner)) {
        //turn on these if they exist and base is a node
        Node *node = object_cast<Node>(p_owner);
        if (p_script->functions.contains("_process"))
            node->set_process(true);
        if (p_script->functions.contains("_physics_process"))
            node->set_physics_process(true);
        if (p_script->functions.contains("_input"))
            node->set_process_input(true);
        if (p_script->functions.contains("_unhandled_input"))
            node->set_process_unhandled_input(true);
        if (p_script->functions.contains("_unhandled_key_input"))
            node->set_process_unhandled_key_input(true);
    }

    for (const eastl::pair<const StringName,VisualScript::Variable> &E : script->variables) {
        variables[E.first] = E.second.default_value;
        //no hacer que todo exporte, solo las que queres!
    }

    for (const eastl::pair<const StringName,VisualScript::Function> &E : script->functions) {
        if (E.first == script->get_default_func()) {
            continue;
        }
        Function function;
        function.node = E.second.function_id;
        function.max_stack = 0;
        function.flow_stack_size = 0;
        function.pass_stack_size = 0;
        function.node_count = 0;

        Map<StringName, int> local_var_indices;

        if (function.node < 0) {
            VisualScriptLanguage::singleton->debug_break_parse(get_script()->get_path(), 0, "No start node in function: " + String(E.first));

            ERR_CONTINUE(function.node < 0);
        }

        {
            Ref<VisualScriptFunction> func_node = dynamic_ref_cast<VisualScriptFunction>(script->get_node(E.first, E.second.function_id));

            if (not func_node) {
                VisualScriptLanguage::singleton->debug_break_parse(get_script()->get_path(), 0, "No VisualScriptFunction typed start node in function: " + String(E.first));
            }

            ERR_CONTINUE(not func_node);

            function.argument_count = func_node->get_argument_count();
            function.max_stack += function.argument_count;
            function.flow_stack_size = func_node->is_stack_less() ? 0 : func_node->get_stack_size();
            max_input_args = M_MAX(max_input_args, function.argument_count);
        }

        //multiple passes are required to set up this complex thing..

        //first create the nodes
        for (const eastl::pair<const int,VisualScript::Function::NodeData> &F : E.second.nodes) {

            Ref<VisualScriptNode> node = F.second.node;
            VisualScriptNodeInstance *instance = node->instance(this); //create instance
            ERR_FAIL_COND(!instance);

            instance->base = node.get();

            instance->id = F.first;
            instance->input_port_count = node->get_input_value_port_count();
            instance->input_ports = nullptr;
            instance->output_port_count = node->get_output_value_port_count();
            instance->output_ports = nullptr;
            instance->sequence_output_count = node->get_output_sequence_port_count();
            instance->sequence_index = function.node_count++;
            instance->sequence_outputs = nullptr;
            instance->pass_idx = -1;

            if (instance->input_port_count) {
                instance->input_ports = memnew_arr(int, instance->input_port_count);
                for (int i = 0; i < instance->input_port_count; i++) {

                    instance->input_ports[i] = -1; //if not assigned, will become default value
                }
            }

            if (instance->output_port_count) {
                instance->output_ports = memnew_arr(int, instance->output_port_count);
                for (int i = 0; i < instance->output_port_count; i++) {
                    instance->output_ports[i] = -1; //if not assigned, will output to trash
                }
            }

            if (instance->sequence_output_count) {
                instance->sequence_outputs = memnew_arr(VisualScriptNodeInstance *, instance->sequence_output_count);
                for (int i = 0; i < instance->sequence_output_count; i++) {
                    instance->sequence_outputs[i] = nullptr; //if it remains null, flow ends here
                }
            }

            if (object_cast<VisualScriptLocalVar>(node.get()) || object_cast<VisualScriptLocalVarSet>(node.get())) {
                //working memory is shared only for this node, for the same variables
                Ref<VisualScriptLocalVar> vslv = dynamic_ref_cast<VisualScriptLocalVar>(node);

                StringName var_name;

                if (object_cast<VisualScriptLocalVar>(node.get()))
                    var_name = StringName(StringUtils::strip_edges(object_cast<VisualScriptLocalVar>(node.get())->get_var_name()));
                else
                    var_name = StringName(StringUtils::strip_edges(object_cast<VisualScriptLocalVarSet>(node.get())->get_var_name()));

                if (!local_var_indices.contains(var_name)) {
                    local_var_indices[var_name] = function.max_stack;
                    function.max_stack++;
                }

                instance->working_mem_idx = local_var_indices[var_name];

            } else if (instance->get_working_memory_size()) {
                instance->working_mem_idx = function.max_stack;
                function.max_stack += instance->get_working_memory_size();
            } else {
                instance->working_mem_idx = -1; //no working mem
            }

            max_input_args = M_MAX(max_input_args, instance->input_port_count);
            max_output_args = M_MAX(max_output_args, instance->output_port_count);

            instances[F.first] = instance;
        }

        function.trash_pos = function.max_stack++; //create pos for trash

        //second pass, do data connections

        for (const VisualScript::DataConnection &dc : E.second.data_connections) {

            ERR_CONTINUE(!instances.contains(dc.from_node));
            VisualScriptNodeInstance *from = instances[dc.from_node];
            ERR_CONTINUE(!instances.contains(dc.to_node));
            VisualScriptNodeInstance *to = instances[dc.to_node];
            ERR_CONTINUE(dc.from_port >= from->output_port_count);
            ERR_CONTINUE(dc.to_port >= to->input_port_count);

            if (from->output_ports[dc.from_port] == -1) {

                int stack_pos = function.max_stack++;
                from->output_ports[dc.from_port] = stack_pos;
            }

            if (from->get_sequence_output_count() == 0 and not to->dependencies.contains(from)) {
                //if the node we are reading from has no output sequence, we must call step() before reading from it.
                if (from->pass_idx == -1) {
                    from->pass_idx = function.pass_stack_size;
                    function.pass_stack_size++;
                }
                to->dependencies.push_back(from);
            }

            to->input_ports[dc.to_port] = from->output_ports[dc.from_port]; //read from wherever the stack is
        }

        //third pass, do sequence connections

        for (const VisualScript::SequenceConnection &sc : E.second.sequence_connections) {

            ERR_CONTINUE(!instances.contains(sc.from_node));
            VisualScriptNodeInstance *from = instances[sc.from_node];
            ERR_CONTINUE(!instances.contains(sc.to_node));
            VisualScriptNodeInstance *to = instances[sc.to_node];
            ERR_CONTINUE(sc.from_output >= from->sequence_output_count);

            from->sequence_outputs[sc.from_output] = to;
        }

        //fourth pass:
        // 1) unassigned input ports to default values
        // 2) connect unassigned output ports to trash

        for (const eastl::pair<const int,VisualScript::Function::NodeData> &F : E.second.nodes) {

            ERR_CONTINUE(!instances.contains(F.first));

            Ref<VisualScriptNode> node = F.second.node;
            VisualScriptNodeInstance *instance = instances[F.first];

            // connect to default values
            for (int i = 0; i < instance->input_port_count; i++) {
                if (instance->input_ports[i] == -1) {

                    //unassigned, connect to default val
                    instance->input_ports[i] = default_values.size() | VisualScriptNodeInstance::INPUT_DEFAULT_VALUE_BIT;
                    default_values.emplace_back(node->get_default_input_value(i));
                }
            }

            // connect to trash
            for (int i = 0; i < instance->output_port_count; i++) {
                if (instance->output_ports[i] == -1) {
                    instance->output_ports[i] = function.trash_pos; //trash is same for all
                }
            }
        }

        functions[E.first] = function;
    }
}

ScriptLanguage *VisualScriptInstance::get_language() {

    return VisualScriptLanguage::singleton;
}

VisualScriptInstance::VisualScriptInstance() {
}

VisualScriptInstance::~VisualScriptInstance() {

    if (VisualScriptLanguage::singleton->lock)
        VisualScriptLanguage::singleton->lock->lock();

    script->instances.erase(owner);

    if (VisualScriptLanguage::singleton->lock)
        VisualScriptLanguage::singleton->lock->unlock();

    for (eastl::pair<const int,VisualScriptNodeInstance *> &E : instances) {
        memdelete(E.second);
    }
}

/////////////////////////////////////////////

/////////////////////

Variant VisualScriptFunctionState::_signal_callback(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    ERR_FAIL_COND_V(function == StringName(), Variant());

#ifdef DEBUG_ENABLED

    ERR_FAIL_COND_V_MSG(instance_id.is_valid() && !gObjectDB().get_instance(instance_id), Variant(), "Resumed after yield, but class instance is gone.");
    ERR_FAIL_COND_V_MSG(script_id.is_valid() && !gObjectDB().get_instance(script_id), Variant(), "Resumed after yield, but script is gone.");

#endif

    r_error.error = Callable::CallError::CALL_OK;

    Array args;

    if (p_argcount == 0) {
        r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
        r_error.argument = 1;
        return Variant();
    } else if (p_argcount == 1) {
        //noooneee, reserved for me, me and only me.
    } else {

        for (int i = 0; i < p_argcount - 1; i++) {
            args.push_back(*p_args[i]);
        }
    }

    Ref<VisualScriptFunctionState> self = refFromVariant<VisualScriptFunctionState>(*p_args[p_argcount - 1]); //hi, I'm myself, needed this to remain alive.

    if (not self) {
        r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
        r_error.argument = p_argcount - 1;
        r_error.expected = VariantType::OBJECT;
        return Variant();
    }

    r_error.error = Callable::CallError::CALL_OK;

    Variant *working_mem = ((Variant *)stack.data()) + working_mem_index;

    *working_mem = args; //arguments go to working mem.

    Variant ret = instance->_call_internal(function, stack.data(), stack.size(), node, flow_stack_pos, pass, true, r_error);
    function = StringName(); //invalidate
    return ret;
}

void VisualScriptFunctionState::connect_to_signal(Object *p_obj, StringView p_signal, Array p_binds) {

    Vector<Variant> binds;
    for (int i = 0; i < p_binds.size(); i++) {
        binds.push_back(p_binds[i]);
    }
    binds.push_back(Ref<VisualScriptFunctionState>(this)); //add myself on the back to avoid dying from unreferencing
    p_obj->connect(StringName(p_signal), this, "_signal_callback", binds, ObjectNS::CONNECT_ONESHOT);
}
void VisualScriptFunctionState::connect_to_signal_sv(Object *p_obj, StringView p_signal, Array p_binds) {

    Vector<Variant> binds;
    for (int i = 0; i < p_binds.size(); i++) {
        binds.push_back(p_binds[i]);
    }
    binds.push_back(Ref<VisualScriptFunctionState>(this)); //add myself on the back to avoid dying from unreferencing
    p_obj->connect(StringName(p_signal), this, "_signal_callback", binds, ObjectNS::CONNECT_ONESHOT);
}

bool VisualScriptFunctionState::is_valid() const {

    return function != StringName();
}

Variant VisualScriptFunctionState::resume(Array p_args) {

    ERR_FAIL_COND_V(function == StringName(), Variant());
#ifdef DEBUG_ENABLED

    ERR_FAIL_COND_V_MSG(instance_id.is_valid() && !gObjectDB().get_instance(instance_id), Variant(), "Resumed after yield, but class instance is gone.");
    ERR_FAIL_COND_V_MSG(script_id.is_valid() && !gObjectDB().get_instance(script_id), Variant(), "Resumed after yield, but script is gone.");

#endif

    Callable::CallError r_error;
    r_error.error = Callable::CallError::CALL_OK;

    Variant *working_mem = ((Variant *)stack.data()) + working_mem_index;

    *working_mem = p_args; //arguments go to working mem.

    Variant ret = instance->_call_internal(function, stack.data(), stack.size(), node, flow_stack_pos, pass, true, r_error);
    function = StringName(); //invalidate
    return ret;
}

void VisualScriptFunctionState::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("connect_to_signal", {"obj", "signals", "args"}), &VisualScriptFunctionState::connect_to_signal);
    MethodBinder::bind_method(D_METHOD("resume", {"args"}), &VisualScriptFunctionState::resume, {DEFVAL(Variant())});
    MethodBinder::bind_method(D_METHOD("is_valid"), &VisualScriptFunctionState::is_valid);
    MethodBinder::bind_vararg_method("_signal_callback", &VisualScriptFunctionState::_signal_callback, MethodInfo("_signal_callback"));
}

VisualScriptFunctionState::VisualScriptFunctionState() {
}

VisualScriptFunctionState::~VisualScriptFunctionState() {

    if (function != StringName()) {
        Variant *s = ((Variant *)stack.data());
        for (int i = 0; i < variant_stack_size; i++) {
            s[i].~Variant();
        }
    }
}

///////////////////////////////////////////////

StringName VisualScriptLanguage::get_name() const {

    return StringName("VisualScript");
}

/* LANGUAGE FUNCTIONS */
bool VisualScriptLanguage::init() {
    return true;
}
String VisualScriptLanguage::get_type() const {

    return ("VisualScript");
}
String VisualScriptLanguage::get_extension() const {

    return String("vs");
}
Error VisualScriptLanguage::execute_file(StringView p_path) {

    return OK;
}
void VisualScriptLanguage::finish() {
}

/* EDITOR FUNCTIONS */
void VisualScriptLanguage::get_reserved_words(Vector<String> *p_words) const {
}
void VisualScriptLanguage::get_comment_delimiters(Vector<String> *p_delimiters) const {
}
void VisualScriptLanguage::get_string_delimiters(Vector<String> *p_delimiters) const {
}
Ref<Script> VisualScriptLanguage::get_template(StringView p_class_name, StringView p_base_class_name) const {

    Ref<VisualScript> script(make_ref_counted<VisualScript>());
    script->set_instance_base_type(StringName(p_base_class_name));
    return script;
}

bool VisualScriptLanguage::is_using_templates() {

    return true;
}

void VisualScriptLanguage::make_template(StringView p_class_name, StringView p_base_class_name, const Ref<Script> &p_script) {
    Ref<VisualScript> script = dynamic_ref_cast<VisualScript>(p_script);
    script->set_instance_base_type(StringName(p_base_class_name));
}

bool VisualScriptLanguage::validate(StringView p_script, int &r_line_error, int &r_col_error, String &r_test_error, StringView p_path, Vector<
        String> *r_functions, Vector<ScriptLanguage::Warning> *r_warnings, Set<int> *r_safe_lines) const {

    return false;
}
Script *VisualScriptLanguage::create_script() const {

    return memnew(VisualScript);
}
bool VisualScriptLanguage::has_named_classes() const {

    return false;
}
bool VisualScriptLanguage::supports_builtin_mode() const {

    return true;
}
int VisualScriptLanguage::find_function(StringView p_function, StringView p_code) const {

    return -1;
}
String VisualScriptLanguage::make_function(const String &p_class, const StringName &p_name, const PoolVector<String> &p_args) const {

    return String();
}

void VisualScriptLanguage::auto_indent_code(String &p_code, int p_from_line, int p_to_line) const {
}
void VisualScriptLanguage::add_global_constant(const StringName &p_variable, const Variant &p_value) {
}

/* DEBUGGER FUNCTIONS */

bool VisualScriptLanguage::debug_break_parse(StringView p_file, int p_node, StringView p_error) {
    //break because of parse error

    if (ScriptDebugger::get_singleton() && Thread::get_caller_id() == Thread::get_main_id()) {

        _debug_parse_err_node = p_node;
        _debug_parse_err_file = p_file;
        _debug_error = p_error;
        ScriptDebugger::get_singleton()->debug(this, false, true);
        return true;
    } else {
        return false;
    }
}

bool VisualScriptLanguage::debug_break(StringView p_error, bool p_allow_continue) {

    if (ScriptDebugger::get_singleton() && Thread::get_caller_id() == Thread::get_main_id()) {

        _debug_parse_err_node = -1;
        _debug_parse_err_file = "";
        _debug_error = p_error;
        ScriptDebugger::get_singleton()->debug(this, p_allow_continue, true);
        return true;
    } else {
        return false;
    }
}

const String &VisualScriptLanguage::debug_get_error() const {

    return _debug_error;
}

int VisualScriptLanguage::debug_get_stack_level_count() const {

    if (_debug_parse_err_node >= 0)
        return 1;

    return _debug_call_stack_pos;
}
int VisualScriptLanguage::debug_get_stack_level_line(int p_level) const {

    if (_debug_parse_err_node >= 0)
        return _debug_parse_err_node;

    ERR_FAIL_INDEX_V(p_level, _debug_call_stack_pos, -1);

    int l = _debug_call_stack_pos - p_level - 1;

    return *(_call_stack[l].current_id);
}
String VisualScriptLanguage::debug_get_stack_level_function(int p_level) const {

    if (_debug_parse_err_node >= 0)
        return null_string;

    ERR_FAIL_INDEX_V(p_level, _debug_call_stack_pos, null_string);
    int l = _debug_call_stack_pos - p_level - 1;
    return _call_stack[l].function->asCString();
}
String VisualScriptLanguage::debug_get_stack_level_source(int p_level) const {

    if (_debug_parse_err_node >= 0)
        return _debug_parse_err_file;

    ERR_FAIL_INDEX_V(p_level, _debug_call_stack_pos, String());
    int l = _debug_call_stack_pos - p_level - 1;
    return _call_stack[l].instance->get_script_ptr()->get_path();
}
void VisualScriptLanguage::debug_get_stack_level_locals(int p_level, Vector<String> *p_locals, Vector<Variant> *p_values, int p_max_subitems, int p_max_depth) {

    if (_debug_parse_err_node >= 0)
        return;

    ERR_FAIL_INDEX(p_level, _debug_call_stack_pos);

    int l = _debug_call_stack_pos - p_level - 1;
    const StringName *f = _call_stack[l].function;

    ERR_FAIL_COND(!_call_stack[l].instance->functions.contains(*f));
    //VisualScriptInstance::Function *func = &_call_stack[l].instance->functions[*f];

    VisualScriptNodeInstance *node = _call_stack[l].instance->instances[*_call_stack[l].current_id];
    ERR_FAIL_COND(!node);

    p_locals->push_back("node_name");
    p_values->push_back(node->get_base_node()->get_text());

    for (int i = 0; i < node->input_port_count; i++) {
        String name = node->get_base_node()->get_input_value_port_info(i).name.asCString();
        if (name.empty()) {
            name = "in_" + itos(i);
        }

        p_locals->push_back("input/" + name);

        //value is trickier

        int in_from = node->input_ports[i];
        int in_value = in_from & VisualScriptNodeInstance::INPUT_MASK;

        if (in_from & VisualScriptNodeInstance::INPUT_DEFAULT_VALUE_BIT) {
            p_values->push_back(_call_stack[l].instance->default_values[in_value]);
        } else {
            p_values->push_back(_call_stack[l].stack[in_value]);
        }
    }

    for (int i = 0; i < node->output_port_count; i++) {

        String name = node->get_base_node()->get_output_value_port_info(i).name.asCString();
        if (name.empty()) {
            name = "out_" + itos(i);
        }

        p_locals->push_back("output/" + name);

        //value is trickier

        int in_from = node->output_ports[i];
        p_values->push_back(_call_stack[l].stack[in_from]);
    }

    for (int i = 0; i < node->get_working_memory_size(); i++) {
        p_locals->push_back("working_mem/mem_" + itos(i));
        p_values->push_back((*_call_stack[l].work_mem)[i]);
    }

    /*
    ERR_FAIL_INDEX(p_level,_debug_call_stack_pos);


    VisualFunction *f = _call_stack[l].function;

    List<Pair<StringName,int> > locals;

    f->debug_get_stack_member_state(*_call_stack[l].line,&locals);
    for( List<Pair<StringName,int> >::Element *E = locals.front();E;E=E->next() ) {

    p_locals->push_back(E.second.first);
    p_values->push_back(_call_stack[l].stack[E.second.second]);
    }
*/
}
void VisualScriptLanguage::debug_get_stack_level_members(int p_level, Vector<String> *p_members, Vector<Variant> *p_values, int p_max_subitems, int p_max_depth) {

    if (_debug_parse_err_node >= 0)
        return;

    ERR_FAIL_INDEX(p_level, _debug_call_stack_pos);
    int l = _debug_call_stack_pos - p_level - 1;

    Ref<VisualScript> vs = dynamic_ref_cast<VisualScript>(_call_stack[l].instance->get_script());
    if (not vs)
        return;

    Vector<StringName> vars;
    vs->get_variable_list(&vars);
    for (int i=0,fin=vars.size(); i<fin; ++i) {
        Variant v;
        if (_call_stack[l].instance->get_variable(vars[i], &v)) {
            p_members->emplace_back(String("variables/") + vars[i]);
            p_values->push_back(v);
        }
    }
}

void VisualScriptLanguage::debug_get_globals(Vector<String> *p_locals, Vector<Variant> *p_values, int p_max_subitems, int p_max_depth) {

    //no globals are really reachable in gdscript
}
String VisualScriptLanguage::debug_parse_stack_level_expression(int p_level, StringView p_expression, int p_max_subitems, int p_max_depth) {

    return String();
}

void VisualScriptLanguage::reload_all_scripts() {
}
void VisualScriptLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
}
/* LOADER FUNCTIONS */

void VisualScriptLanguage::get_recognized_extensions(Vector<String> *p_extensions) const {

    p_extensions->emplace_back("vs");
}
void VisualScriptLanguage::get_public_functions(Vector<MethodInfo> *p_functions) const {
}
void VisualScriptLanguage::get_public_constants(Vector<Pair<StringView, Variant>> *p_constants) const {
}

void VisualScriptLanguage::profiling_start() {
}
void VisualScriptLanguage::profiling_stop() {
}

int VisualScriptLanguage::profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) {

    return 0;
}

int VisualScriptLanguage::profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) {

    return 0;
}

VisualScriptLanguage *VisualScriptLanguage::singleton = nullptr;

void VisualScriptLanguage::add_register_func(StringView p_name, VisualScriptNodeRegisterFunc p_func) {

    ERR_FAIL_COND(register_funcs.contains_as(p_name));
    register_funcs.emplace(String(p_name),p_func);
}
void VisualScriptLanguage::remove_register_func(StringView p_name) {
    ERR_FAIL_COND(!register_funcs.contains_as(p_name));
    register_funcs.erase(String(p_name));
}

Ref<VisualScriptNode> VisualScriptLanguage::create_node_from_name(const String &p_name) {

    ERR_FAIL_COND_V(!register_funcs.contains(p_name), Ref<VisualScriptNode>());

    return register_funcs[p_name](p_name);
}

void VisualScriptLanguage::get_registered_node_names(Vector<String> *r_names) {
    assert(r_names && r_names->empty()); // we are overwriting provided container, make sure it's empty.
    *r_names = eastl::move(register_funcs.keys());
}

VisualScriptLanguage::VisualScriptLanguage() {

    notification = "_notification";
    _step = "_step";
    _subcall = "_subcall";
    singleton = this;
#ifndef NO_THREADS
    lock = memnew(Mutex);
#endif

    _debug_parse_err_node = -1;
    _debug_parse_err_file = "";
    _debug_call_stack_pos = 0;
    int dmcs = T_GLOBAL_DEF("debug/settings/visual_script/max_call_stack", int(1024));
    ProjectSettings::get_singleton()->set_custom_property_info("debug/settings/visual_script/max_call_stack", PropertyInfo(VariantType::INT, "debug/settings/visual_script/max_call_stack", PropertyHint::Range, "1024,4096,1,or_greater")); //minimum is 1024

    if (ScriptDebugger::get_singleton()) {
        //debugging enabled!
        _debug_max_call_stack = dmcs;
        _call_stack = memnew_arr(CallLevel, _debug_max_call_stack + 1);

    } else {
        _debug_max_call_stack = 0;
        _call_stack = nullptr;
    }
}

VisualScriptLanguage::~VisualScriptLanguage() {

    if (lock)
        memdelete(lock);

    if (_call_stack) {
        memdelete_arr(_call_stack);
    }
    singleton = nullptr;
}