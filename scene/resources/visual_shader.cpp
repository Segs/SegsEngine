/*************************************************************************/
/*  visual_shader.cpp                                                    */
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

#include "visual_shader.h"

#include "core/method_bind.h"
#include "core/vmap.h"
#include "core/string_formatter.h"
#include "core/translation_helpers.h"
#include "core/object_tooling.h"
#include "scene/resources/shader_enum_casters.h"
#include "servers/visual/shader_types.h"
#include "core/string_utils.inl"
using namespace eastl;
IMPL_GDCLASS(VisualShader)
IMPL_GDCLASS(VisualShaderNode)
IMPL_GDCLASS(VisualShaderNodeUniform)
IMPL_GDCLASS(VisualShaderNodeCustom)
IMPL_GDCLASS(VisualShaderNodeInput)
IMPL_GDCLASS(VisualShaderNodeOutput)
IMPL_GDCLASS(VisualShaderNodeGroupBase)
IMPL_GDCLASS(VisualShaderNodeExpression)
IMPL_GDCLASS(VisualShaderNodeGlobalExpression)

VARIANT_ENUM_CAST(VisualShader::Type)
VARIANT_ENUM_CAST(VisualShaderNode::PortType)

void VisualShaderNode::set_output_port_for_preview(int p_index) {

    port_preview = p_index;
}

int VisualShaderNode::get_output_port_for_preview() const {

    return port_preview;
}

void VisualShaderNode::set_input_port_default_value(int p_port, const Variant &p_value) {
    default_input_values[p_port] = p_value;
    emit_changed();
}

Variant VisualShaderNode::get_input_port_default_value(int p_port) const {
    if (default_input_values.contains(p_port)) {
        return default_input_values.at(p_port);
    }

    return Variant();
}

bool VisualShaderNode::is_port_separator(int /*p_index*/) const {
    return false;
}

Vector<VisualShader::DefaultTextureParam> VisualShaderNode::get_default_texture_parameters(VisualShader::Type p_type, int p_id) const {
    return Vector<VisualShader::DefaultTextureParam>();
}
String VisualShaderNode::generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {
    return String();
}

String VisualShaderNode::generate_global_per_node(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {
    return String();
}

String VisualShaderNode::generate_global_per_func(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {
    return String();
}

Vector<StringName> VisualShaderNode::get_editable_properties() const {
    return Vector<StringName>();
}

Array VisualShaderNode::_get_default_input_values() const {

    Array ret;
    for (const eastl::pair<const int,Variant> &E : default_input_values) {
        ret.push_back(E.first);
        ret.push_back(E.second);
    }
    return ret;
}
void VisualShaderNode::_set_default_input_values(const Array &p_values) {

    if (p_values.size() % 2 == 0) {
        for (int i = 0; i < p_values.size(); i += 2) {
            default_input_values[p_values[i + 0]] = p_values[i + 1];
        }
    }

    emit_changed();
}

StringName VisualShaderNode::get_warning(ShaderMode p_mode, VisualShader::Type p_type) const {
    return StringName();
}

StringName VisualShaderNode::get_input_port_default_hint(int p_port) const {
    return StringName();
}

void VisualShaderNode::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_output_port_for_preview", {"port"}), &VisualShaderNode::set_output_port_for_preview);
    MethodBinder::bind_method(D_METHOD("get_output_port_for_preview"), &VisualShaderNode::get_output_port_for_preview);

    MethodBinder::bind_method(D_METHOD("set_input_port_default_value", {"port", "value"}), &VisualShaderNode::set_input_port_default_value);
    MethodBinder::bind_method(D_METHOD("get_input_port_default_value", {"port"}), &VisualShaderNode::get_input_port_default_value);

    MethodBinder::bind_method(D_METHOD("_set_default_input_values", {"values"}), &VisualShaderNode::_set_default_input_values);
    MethodBinder::bind_method(D_METHOD("_get_default_input_values"), &VisualShaderNode::_get_default_input_values);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "output_port_for_preview"), "set_output_port_for_preview", "get_output_port_for_preview");
    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "default_input_values", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "_set_default_input_values", "_get_default_input_values");
    ADD_SIGNAL(MethodInfo("editor_refresh_request"));

    BIND_ENUM_CONSTANT(PORT_TYPE_SCALAR)
    BIND_ENUM_CONSTANT(PORT_TYPE_VECTOR)
    BIND_ENUM_CONSTANT(PORT_TYPE_BOOLEAN)
    BIND_ENUM_CONSTANT(PORT_TYPE_TRANSFORM)
    BIND_ENUM_CONSTANT(PORT_TYPE_SAMPLER)
    BIND_ENUM_CONSTANT(PORT_TYPE_MAX)
}

VisualShaderNode::VisualShaderNode() {
    port_preview = -1;
}

/////////////////////////////////////////////////////////

void VisualShaderNodeCustom::update_ports() {
    ERR_FAIL_COND(!get_script_instance())

    input_ports.clear();
    if (get_script_instance()->has_method("_get_input_port_count")) {
        int input_port_count = (int)get_script_instance()->call("_get_input_port_count");
        bool has_name = get_script_instance()->has_method("_get_input_port_name");
        bool has_type = get_script_instance()->has_method("_get_input_port_type");
        for (int i = 0; i < input_port_count; i++) {
            Port port;
            if (has_name) {
                port.name = StringName(get_script_instance()->call("_get_input_port_name", i));
            } else {
                port.name = StringName("in" + itos(i));
            }
            if (has_type) {
                port.type = (int)get_script_instance()->call("_get_input_port_type", i);
            } else {
                port.type = (int)PortType::PORT_TYPE_SCALAR;
            }
            input_ports.push_back(port);
        }
    }
    output_ports.clear();
    if (get_script_instance()->has_method("_get_output_port_count")) {
        int output_port_count = (int)get_script_instance()->call("_get_output_port_count");
        bool has_name = get_script_instance()->has_method("_get_output_port_name");
        bool has_type = get_script_instance()->has_method("_get_output_port_type");
        for (int i = 0; i < output_port_count; i++) {
            Port port;
            if (has_name) {
                port.name = StringName(get_script_instance()->call("_get_output_port_name", i));
            } else {
                port.name = StringName("out" + itos(i));
            }
            if (has_type) {
                port.type = (int)get_script_instance()->call("_get_output_port_type", i);
            } else {
                port.type = (int)PortType::PORT_TYPE_SCALAR;
            }
            output_ports.push_back(port);
        }
    }
}

se_string_view VisualShaderNodeCustom::get_caption() const {
    ERR_FAIL_COND_V(!get_script_instance(), null_se_string)
    if (get_script_instance()->has_method("_get_name")) {
        return get_script_instance()->call("_get_name").as<String>();
    }
    return String("Unnamed");
}

int VisualShaderNodeCustom::get_input_port_count() const {
    return input_ports.size();
}

VisualShaderNodeCustom::PortType VisualShaderNodeCustom::get_input_port_type(int p_port) const {
    ERR_FAIL_INDEX_V(p_port, input_ports.size(), PORT_TYPE_SCALAR)
    return (PortType)input_ports[p_port].type;
}

StringName VisualShaderNodeCustom::get_input_port_name(int p_port) const {
    ERR_FAIL_INDEX_V(p_port, input_ports.size(), StringName())
    return input_ports[p_port].name;
}

int VisualShaderNodeCustom::get_output_port_count() const {
    return output_ports.size();
}

VisualShaderNodeCustom::PortType VisualShaderNodeCustom::get_output_port_type(int p_port) const {
    ERR_FAIL_INDEX_V(p_port, output_ports.size(), PORT_TYPE_SCALAR)
    return (PortType)output_ports[p_port].type;
}

StringName VisualShaderNodeCustom::get_output_port_name(int p_port) const {
    ERR_FAIL_INDEX_V(p_port, output_ports.size(), StringName())
    return output_ports[p_port].name;
}

String VisualShaderNodeCustom::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    ERR_FAIL_COND_V(!get_script_instance(), String())
    ERR_FAIL_COND_V(!get_script_instance()->has_method("_get_code"), String())
    Array input_vars;
    for (int i = 0; i < get_input_port_count(); i++) {
        input_vars.push_back(p_input_vars[i]);
    }
    Array output_vars;
    for (int i = 0; i < get_output_port_count(); i++) {
        output_vars.push_back(p_output_vars[i]);
    }
    String code("\t{\n");
    String _code = (String)get_script_instance()->call("_get_code", input_vars, output_vars, (int)p_mode, (int)p_type);
    bool nend =StringUtils::ends_with( _code,"\n");
    _code = StringUtils::insert(_code,0, String("\t\t"));
    _code = StringUtils::replace(_code,"\n", "\n\t\t");
    code += _code;
    if (!nend) {
        code += String("\n\t}");
    } else {
        StringUtils::erase(code,code.size() - 1,1);
        code += '}';
    }
    return code;
}

String VisualShaderNodeCustom::generate_global_per_node(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {
    ERR_FAIL_COND_V(!get_script_instance(), String())
    if (get_script_instance()->has_method("_get_global_code")) {
        String code = String("// ") + get_caption() + "\n";
        code += (String)get_script_instance()->call("_get_global_code", (int)p_mode);
        code += '\n';
        return code;
    }
    return String();
}

void VisualShaderNodeCustom::_bind_methods() {

    BIND_VMETHOD(MethodInfo(VariantType::STRING, "_get_name"))
    BIND_VMETHOD(MethodInfo(VariantType::STRING, "_get_description"))
    BIND_VMETHOD(MethodInfo(VariantType::STRING, "_get_category"))
    BIND_VMETHOD(MethodInfo(VariantType::STRING, "_get_subcategory"))
    BIND_VMETHOD(MethodInfo(VariantType::INT, "_get_return_icon_type"))
    BIND_VMETHOD(MethodInfo(VariantType::INT, "_get_input_port_count"))
    BIND_VMETHOD(MethodInfo(VariantType::INT, "_get_input_port_type", PropertyInfo(VariantType::INT, "port")))
    BIND_VMETHOD(MethodInfo(VariantType::STRING, "_get_input_port_name", PropertyInfo(VariantType::INT, "port")))
    BIND_VMETHOD(MethodInfo(VariantType::INT, "_get_output_port_count"))
    BIND_VMETHOD(MethodInfo(VariantType::INT, "_get_output_port_type", PropertyInfo(VariantType::INT, "port")))
    BIND_VMETHOD(MethodInfo(VariantType::STRING, "_get_output_port_name", PropertyInfo(VariantType::INT, "port")))
    BIND_VMETHOD(MethodInfo(VariantType::STRING, "_get_code", PropertyInfo(VariantType::ARRAY, "input_vars"), PropertyInfo(VariantType::ARRAY, "output_vars"), PropertyInfo(VariantType::INT, "mode"), PropertyInfo(VariantType::INT, "type")))
    BIND_VMETHOD(MethodInfo(VariantType::STRING, "_get_global_code", PropertyInfo(VariantType::INT, "mode")))
}

VisualShaderNodeCustom::VisualShaderNodeCustom() {
}

/////////////////////////////////////////////////////////

void VisualShader::add_node(Type p_type, const Ref<VisualShaderNode> &p_node, const Vector2 &p_position, int p_id) {
    ERR_FAIL_COND(not p_node)
    ERR_FAIL_COND(p_id < 2)
    ERR_FAIL_INDEX(p_type, TYPE_MAX)
    Graph *g = &graph[p_type];
    ERR_FAIL_COND(g->nodes.contains(p_id))
    Node n;
    n.node = p_node;
    n.position = p_position;

    Ref<VisualShaderNodeUniform> uniform = dynamic_ref_cast<VisualShaderNodeUniform>(n.node);
    if (uniform) {
        String valid_name = validate_uniform_name(uniform->get_uniform_name(), uniform);
        uniform->set_uniform_name(StringName(valid_name));
    }

    Ref<VisualShaderNodeInput> input = dynamic_ref_cast<VisualShaderNodeInput>(n.node);
    if (input) {
        input->shader_mode = shader_mode;
        input->shader_type = p_type;
        input->connect("input_type_changed", this, "_input_type_changed", varray(p_type, p_id));
    }

    n.node->connect("changed", this, "_queue_update");

    Ref<VisualShaderNodeCustom> custom = dynamic_ref_cast<VisualShaderNodeCustom>(n.node);
    if (custom) {
        custom->update_ports();
    }

    g->nodes[p_id] = n;

    _queue_update();
}

void VisualShader::set_node_position(Type p_type, int p_id, const Vector2 &p_position) {
    ERR_FAIL_INDEX(p_type, TYPE_MAX)
    Graph *g = &graph[p_type];
    ERR_FAIL_COND(!g->nodes.contains(p_id))
    g->nodes[p_id].position = p_position;
}

Vector2 VisualShader::get_node_position(Type p_type, int p_id) const {
    ERR_FAIL_INDEX_V(p_type, TYPE_MAX, Vector2())
    const Graph *g = &graph[p_type];
    ERR_FAIL_COND_V(!g->nodes.contains(p_id), Vector2())
    return g->nodes.at(p_id).position;
}

Ref<VisualShaderNode> VisualShader::get_node(Type p_type, int p_id) const {
    ERR_FAIL_INDEX_V(p_type, TYPE_MAX, Ref<VisualShaderNode>())
    const Graph *g = &graph[p_type];
    ERR_FAIL_COND_V(!g->nodes.contains(p_id), Ref<VisualShaderNode>())
    return g->nodes.at(p_id).node;
}

Vector<int> VisualShader::get_node_list(Type p_type) const {
    ERR_FAIL_INDEX_V(p_type, TYPE_MAX, Vector<int>())
    const Graph *g = &graph[p_type];

    Vector<int> ret;
    for (const eastl::pair<const int,Node> &E : g->nodes) {
        ret.push_back(E.first);
    }

    return ret;
}
int VisualShader::get_valid_node_id(Type p_type) const {
    ERR_FAIL_INDEX_V(p_type, TYPE_MAX, NODE_ID_INVALID)
    const Graph *g = &graph[p_type];
    return !g->nodes.empty() ? MAX(2, g->nodes.rbegin()->first + 1) : 2;
}

int VisualShader::find_node_id(Type p_type, const Ref<VisualShaderNode> &p_node) const {
    for (const auto &E : graph[p_type].nodes) {
        if (E.second.node == p_node)
            return E.first;
    }

    return NODE_ID_INVALID;
}

void VisualShader::remove_node(Type p_type, int p_id) {
    ERR_FAIL_INDEX(p_type, TYPE_MAX);
    ERR_FAIL_COND(p_id < 2)
    Graph *g = &graph[p_type];
    ERR_FAIL_COND(!g->nodes.contains(p_id))

    Ref<VisualShaderNodeInput> input = dynamic_ref_cast<VisualShaderNodeInput>(g->nodes[p_id].node);
    if (input) {
        input->disconnect("input_type_changed", this, "_input_type_changed");
    }

    g->nodes[p_id].node->disconnect("changed", this, "_queue_update");

    g->nodes.erase(p_id);

    for (List<Connection>::Element *E = g->connections.front(); E;) {
        List<Connection>::Element *N = E->next();
        if (E->deref().from_node == p_id || E->deref().to_node == p_id) {
            g->connections.erase(E);
        }
        E = N;
    }

    _queue_update();
}

bool VisualShader::is_node_connection(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) const {
    ERR_FAIL_INDEX_V(p_type, TYPE_MAX, false);
    const Graph *g = &graph[p_type];

    for (const List<Connection>::Element *E = g->connections.front(); E; E = E->next()) {

        if (E->deref().from_node == p_from_node && E->deref().from_port == p_from_port && E->deref().to_node == p_to_node && E->deref().to_port == p_to_port) {
            return true;
        }
    }

    return false;
}

bool VisualShader::can_connect_nodes(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) const {

    ERR_FAIL_INDEX_V(p_type, TYPE_MAX, false);
    const Graph *g = &graph[p_type];

    if (!g->nodes.contains(p_from_node))
        return false;

    if (p_from_node == p_to_node)
        return false;

    if (p_from_port < 0 || p_from_port >= g->nodes.at(p_from_node).node->get_output_port_count())
        return false;

    if (!g->nodes.contains(p_to_node))
        return false;

    if (p_to_port < 0 || p_to_port >= g->nodes.at(p_from_node).node->get_input_port_count())
        return false;

    VisualShaderNode::PortType from_port_type = g->nodes.at(p_from_node).node->get_output_port_type(p_from_port);
    VisualShaderNode::PortType to_port_type = g->nodes.at(p_from_node).node->get_input_port_type(p_to_port);

    if (!is_port_types_compatible(from_port_type, to_port_type)) {
        return false;
    }

    for (const List<Connection>::Element *E = g->connections.front(); E; E = E->next()) {

        if (E->deref().from_node == p_from_node && E->deref().from_port == p_from_port && E->deref().to_node == p_to_node && E->deref().to_port == p_to_port) {
            return false;
        }
    }

    return true;
}

bool VisualShader::is_port_types_compatible(int p_a, int p_b) const {
    return MAX(0, p_a - 2) == (MAX(0, p_b - 2));
}

void VisualShader::connect_nodes_forced(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) {
    ERR_FAIL_INDEX(p_type, TYPE_MAX);
    Graph *g = &graph[p_type];
    Connection c;
    c.from_node = p_from_node;
    c.from_port = p_from_port;
    c.to_node = p_to_node;
    c.to_port = p_to_port;
    g->connections.push_back(c);
    _queue_update();
}

Error VisualShader::connect_nodes(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) {
    ERR_FAIL_INDEX_V(p_type, TYPE_MAX, ERR_CANT_CONNECT);
    Graph *g = &graph[p_type];

    ERR_FAIL_COND_V(!g->nodes.contains(p_from_node), ERR_INVALID_PARAMETER)
    ERR_FAIL_INDEX_V(p_from_port, g->nodes[p_from_node].node->get_output_port_count(), ERR_INVALID_PARAMETER)
    ERR_FAIL_COND_V(!g->nodes.contains(p_to_node), ERR_INVALID_PARAMETER)
    ERR_FAIL_INDEX_V(p_to_port, g->nodes[p_to_node].node->get_input_port_count(), ERR_INVALID_PARAMETER)

    VisualShaderNode::PortType from_port_type = g->nodes[p_from_node].node->get_output_port_type(p_from_port);
    VisualShaderNode::PortType to_port_type = g->nodes[p_to_node].node->get_input_port_type(p_to_port);

    ERR_FAIL_COND_V_MSG(!is_port_types_compatible(from_port_type, to_port_type), ERR_INVALID_PARAMETER, "Incompatible port types (scalar/vec/bool) with transform.")

    for (List<Connection>::Element *E = g->connections.front(); E; E = E->next()) {

        if (E->deref().from_node == p_from_node && E->deref().from_port == p_from_port && E->deref().to_node == p_to_node && E->deref().to_port == p_to_port) {
            ERR_FAIL_V(ERR_ALREADY_EXISTS)
        }
    }

    Connection c;
    c.from_node = p_from_node;
    c.from_port = p_from_port;
    c.to_node = p_to_node;
    c.to_port = p_to_port;
    g->connections.push_back(c);

    _queue_update();
    return OK;
}

void VisualShader::disconnect_nodes(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) {
    ERR_FAIL_INDEX(p_type, TYPE_MAX);
    Graph *g = &graph[p_type];

    for (List<Connection>::Element *E = g->connections.front(); E; E = E->next()) {

        if (E->deref().from_node == p_from_node && E->deref().from_port == p_from_port && E->deref().to_node == p_to_node && E->deref().to_port == p_to_port) {
            g->connections.erase(E);
            _queue_update();
            return;
        }
    }
}

Array VisualShader::_get_node_connections(Type p_type) const {
    ERR_FAIL_INDEX_V(p_type, TYPE_MAX, Array());
    const Graph *g = &graph[p_type];

    Array ret;
    for (const List<Connection>::Element *E = g->connections.front(); E; E = E->next()) {
        Dictionary d;
        d["from_node"] = E->deref().from_node;
        d["from_port"] = E->deref().from_port;
        d["to_node"] = E->deref().to_node;
        d["to_port"] = E->deref().to_port;
        ret.push_back(d);
    }

    return ret;
}

void VisualShader::get_node_connections(Type p_type, List<Connection> *r_connections) const {
    ERR_FAIL_INDEX(p_type, TYPE_MAX);
    const Graph *g = &graph[p_type];

    for (const List<Connection>::Element *E = g->connections.front(); E; E = E->next()) {
        r_connections->push_back(E->deref());
    }
}

void VisualShader::set_mode(ShaderMode p_mode) {
    if (shader_mode == p_mode) {
        return;
    }

    //erase input/output connections
    modes.clear();
    flags.clear();
    shader_mode = p_mode;
    for (int i = 0; i < TYPE_MAX; i++) {

        for (auto &E : graph[i].nodes) {

            Ref<VisualShaderNodeInput> input = dynamic_ref_cast<VisualShaderNodeInput>(E.second.node);
            if (input) {
                input->shader_mode = shader_mode;
                //input->input_index = 0;
            }
        }

        Ref<VisualShaderNodeOutput> output = dynamic_ref_cast<VisualShaderNodeOutput>(graph[i].nodes[NODE_ID_OUTPUT].node);
        output->shader_mode = shader_mode;

        // clear connections since they are no longer valid
        for (List<Connection>::Element *E = graph[i].connections.front(); E;) {

            bool keep = true;

            List<Connection>::Element *N = E->next();

            int from = E->deref().from_node;
            int to = E->deref().to_node;

            if (!graph[i].nodes.contains(from)) {
                keep = false;
            } else {
                Ref<VisualShaderNode> from_node = graph[i].nodes[from].node;
                if (from_node->is_class("VisualShaderNodeOutput") || from_node->is_class("VisualShaderNodeInput")) {
                    keep = false;
                }
            }

            if (!graph[i].nodes.contains(to)) {
                keep = false;
            } else {
                Ref<VisualShaderNode> to_node = graph[i].nodes[to].node;
                if (to_node->is_class("VisualShaderNodeOutput") || to_node->is_class("VisualShaderNodeInput")) {
                    keep = false;
                }
            }

            if (!keep) {
                graph[i].connections.erase(E);
            }
            E = N;
        }
    }

    _queue_update();
    Object_change_notify(this);
}

void VisualShader::set_graph_offset(const Vector2 &p_offset) {
    graph_offset = p_offset;
}

Vector2 VisualShader::get_graph_offset() const {
    return graph_offset;
}

ShaderMode VisualShader::get_mode() const {
    return shader_mode;
}

bool VisualShader::is_text_shader() const {
    return false;
}

String VisualShader::generate_preview_shader(Type p_type, int p_node, int p_port, Vector<DefaultTextureParam> &default_tex_params) const {

    Ref<VisualShaderNode> node = get_node(p_type, p_node);
    ERR_FAIL_COND_V(not node, String())
    ERR_FAIL_COND_V(p_port < 0 || p_port >= node->get_output_port_count(), String())
    ERR_FAIL_COND_V(node->get_output_port_type(p_port) == VisualShaderNode::PORT_TYPE_TRANSFORM, String())

    StringBuilder global_code;
    StringBuilder global_code_per_node;
    Map<Type, StringBuilder> global_code_per_func;
    StringBuilder code;
    Set<StringName> classes;

    global_code += String() + "shader_type canvas_item;\n";

    String global_expressions;
    for (int i = 0, index = 0; i < TYPE_MAX; i++) {
        for (const eastl::pair<const int, Node> &E : graph[i].nodes) {
            Ref<VisualShaderNodeGlobalExpression> global_expression(dynamic_ref_cast<VisualShaderNodeGlobalExpression>(E.second.node));
            if (global_expression) {

                String expr;
                expr += "// " + global_expression->get_caption() + ":" + itos(index++) + "\n";
                expr += global_expression->generate_global(get_mode(), Type(i), -1);
                expr =StringUtils::replace(expr,"\n", "\n\t");
                expr += '\n';
                global_expressions += expr;
            }
        }
    }

    global_code += "\n";
    global_code += global_expressions;

    //make it faster to go around through shader
    VMap<ConnectionKey, const List<Connection>::Element *> input_connections;
    VMap<ConnectionKey, const List<Connection>::Element *> output_connections;

    for (const List<Connection>::Element *E = graph[p_type].connections.front(); E; E = E->next()) {
        ConnectionKey from_key;
        from_key.node = E->deref().from_node;
        from_key.port = E->deref().from_port;

        output_connections.insert(from_key, E);

        ConnectionKey to_key;
        to_key.node = E->deref().to_node;
        to_key.port = E->deref().to_port;

        input_connections.insert(to_key, E);
    }

    code += "\nvoid fragment() {\n";

    Set<int> processed;
    Error err = _write_node(p_type, global_code, global_code_per_node, global_code_per_func, code, default_tex_params, input_connections, output_connections, p_node, processed, true, classes);
    ERR_FAIL_COND_V(err != OK, String())

    if (node->get_output_port_type(p_port) == VisualShaderNode::PORT_TYPE_SCALAR) {
        code += "\tCOLOR.rgb = vec3( n_out" + itos(p_node) + "p" + itos(p_port) + " );\n";
    } else if (node->get_output_port_type(p_port) == VisualShaderNode::PORT_TYPE_BOOLEAN) {
        code += "\tCOLOR.rgb = vec3( n_out" + itos(p_node) + "p" + itos(p_port) + " ? 1.0 : 0.0 );\n";
    } else {
        code += "\tCOLOR.rgb = n_out" + itos(p_node) + "p" + itos(p_port) + ";\n";
    }
    code += "}\n";

    //set code secretly
    global_code += "\n\n";
    String final_code(global_code);
    final_code += global_code_per_node;
    final_code += code;
    return final_code;
}

#define IS_INITIAL_CHAR(m_d) (((m_d) >= 'a' && (m_d) <= 'z') || ((m_d) >= 'A' && (m_d) <= 'Z'))

#define IS_SYMBOL_CHAR(m_d) (((m_d) >= 'a' && (m_d) <= 'z') || ((m_d) >= 'A' && (m_d) <= 'Z') || ((m_d) >= '0' && (m_d) <= '9') || (m_d) == '_')

String VisualShader::validate_port_name(se_string_view p_name, const List<StringName> &p_input_ports, const List<StringName> &p_output_ports) const {
    String name(p_name);

    while (name.length() && !IS_INITIAL_CHAR(name[0])) {
        name = StringUtils::substr(name,1, name.length() - 1);
    }

    if (!name.empty()) {

        String valid_name;

        for (size_t i = 0; i < name.length(); i++) {
            if (IS_SYMBOL_CHAR(name[i])) {
                valid_name += name[i];
            } else if (name[i] == ' ') {
                valid_name += '_';
            }
        }

        name = valid_name;
    }

    String valid_name = name;
    bool is_equal = false;

    for (int i = 0; i < p_input_ports.size(); i++) {
        if (name == p_input_ports[i]) {
            is_equal = true;
            break;
        }
    }

    if (!is_equal) {
        for (int i = 0; i < p_output_ports.size(); i++) {
            if (name == p_output_ports[i]) {
                is_equal = true;
                break;
            }
        }
    }

    if (is_equal) {
        name = String();
    }

    return name;
}

String VisualShader::validate_uniform_name(se_string_view p_name, const Ref<VisualShaderNodeUniform> &p_uniform) const {

    String name(p_name); //validate name first
    while (name.length() && !IS_INITIAL_CHAR(name[0])) {
        name = StringUtils::substr(name,1, name.length() - 1);
    }
    if (!name.empty()) {

        String valid_name;

        for (size_t i = 0; i < name.length(); i++) {
            if (IS_SYMBOL_CHAR(name[i])) {
                valid_name += name[i];
            } else if (name[i] == ' ') {
                valid_name += '_';
            }
        }

        name = valid_name;
    }

    if (name.empty()) {
        name = p_uniform->get_caption();
    }

    int attempt = 1;

    while (true) {

        bool exists = false;
        for (int i = 0; i < TYPE_MAX; i++) {
            for (const auto &E : graph[i].nodes) {
                Ref<VisualShaderNodeUniform> node = dynamic_ref_cast<VisualShaderNodeUniform>(E.second.node);
                if (node == p_uniform) { //do not test on self
                    continue;
                }
                if (node && node->get_uniform_name() == name) {
                    exists = true;
                    break;
                }
            }
            if (exists) {
                break;
            }
        }

        if (exists) {
            //remove numbers, put new and try again
            attempt++;
            while (name.length() && name[name.length() - 1] >= '0' && name[name.length() - 1] <= '9') {
                name = StringUtils::substr(name,0, name.length() - 1);
            }
            ERR_FAIL_COND_V(name.empty(), String())
            name += itos(attempt);
        } else {
            break;
        }
    }

    return name;
}

VisualShader::RenderModeEnums VisualShader::render_mode_enums[] = {
    { ShaderMode::SPATIAL, "blend" },
    { ShaderMode::SPATIAL, "depth_draw" },
    { ShaderMode::SPATIAL, "cull" },
    { ShaderMode::SPATIAL, "diffuse" },
    { ShaderMode::SPATIAL, "specular" },
    { ShaderMode::CANVAS_ITEM, "blend" },
    { ShaderMode::CANVAS_ITEM, nullptr }
};

static const char *type_string[VisualShader::TYPE_MAX] = {
    "vertex",
    "fragment",
    "light"
};
bool VisualShader::_set(const StringName &p_name, const Variant &p_value) {

    se_string_view name = p_name.asCString();
    if (name == "mode"_sv) {
        set_mode(ShaderMode(int(p_value)));
        return true;
    } else if (StringUtils::begins_with(name,"flags/")) {
        StringName flag(StringUtils::get_slice(name,'/', 1));
        bool enable = p_value;
        if (enable) {
            flags.insert(flag);
        } else {
            flags.erase(flag);
        }
        _queue_update();
        return true;
    } else if (StringUtils::begins_with(name,"modes/")) {
        String mode(StringUtils::get_slice(name,'/', 1));
        int value = p_value;
        if (value == 0) {
            modes.erase(mode); //means it's default anyway, so don't store it
        } else {
            modes[mode] = value;
        }
        _queue_update();
        return true;
    } else if (StringUtils::begins_with(name,"nodes/")) {
        String typestr(StringUtils::get_slice(name,'/', 1));
        Type type = TYPE_VERTEX;
        for (int i = 0; i < TYPE_MAX; i++) {
            if (typestr == type_string[i]) {
                type = Type(i);
                break;
            }
        }

        String index(StringUtils::get_slice(name,'/', 2));
        if (index == "connections") {

            Vector<int> conns = p_value.as<Vector<int>>();
            if (conns.size() % 4 == 0) {
                for (int i = 0; i < conns.size(); i += 4) {
                    connect_nodes_forced(type, conns[i + 0], conns[i + 1], conns[i + 2], conns[i + 3]);
                }
            }
            return true;
        }

        int id = StringUtils::to_int(index);
        se_string_view what(StringUtils::get_slice(name,'/', 3));

        if (what == "node"_sv) {
            add_node(type, refFromRefPtr<VisualShaderNode>(p_value), Vector2(), id);
            return true;
        } else if (what == "position"_sv) {
            set_node_position(type, id, p_value);
            return true;
        } else if (what == "size"_sv) {
            ((VisualShaderNodeGroupBase *)get_node(type, id).get())->set_size(p_value);
            return true;
        } else if (what == "input_ports"_sv) {
            ((VisualShaderNodeGroupBase *)get_node(type, id).get())->set_inputs(p_value);
            return true;
        } else if (what == "output_ports"_sv) {
            ((VisualShaderNodeGroupBase *)get_node(type, id).get())->set_outputs(p_value);
            return true;
        } else if (what == "expression"_sv) {
            ((VisualShaderNodeExpression *)get_node(type, id).get())->set_expression(p_value);
            return true;
        }
    }
    return false;
}

bool VisualShader::_get(const StringName &p_name, Variant &r_ret) const {

    se_string_view name = p_name.asCString();
    if (name == "mode"_sv) {
        r_ret = get_mode();
        return true;
    } else if (StringUtils::begins_with(name,"flags/")) {
        StringName flag(StringUtils::get_slice(name,'/', 1));
        r_ret = flags.contains(flag);
        return true;
    } else if (StringUtils::begins_with(name,"modes/")) {
        se_string_view mode(StringUtils::get_slice(name,'/', 1));
        auto iter = modes.find_as(mode);
        if (modes.end()!=iter) {
            r_ret = iter->second;
        } else {
            r_ret = 0;
        }
        return true;
    } else if (StringUtils::begins_with(name,"nodes/")) {
        String typestr(StringUtils::get_slice(name,'/', 1));
        Type type = TYPE_VERTEX;
        for (int i = 0; i < TYPE_MAX; i++) {
            if (typestr == type_string[i]) {
                type = Type(i);
                break;
            }
        }

        String index(StringUtils::get_slice(name,'/', 2));
        if (index == "connections") {

            PODVector<int> conns;
            for (const List<Connection>::Element *E = graph[type].connections.front(); E; E = E->next()) {
                conns.push_back(E->deref().from_node);
                conns.push_back(E->deref().from_port);
                conns.push_back(E->deref().to_node);
                conns.push_back(E->deref().to_port);
            }

            r_ret = conns;
            return true;
        }

        int id = StringUtils::to_int(index);
        se_string_view what(StringUtils::get_slice(name,'/', 3));

        if (what == "node"_sv) {
            r_ret = get_node(type, id);
            return true;
        } else if (what == "position"_sv) {
            r_ret = get_node_position(type, id);
            return true;
        } else if (what == "size"_sv) {
            r_ret = ((VisualShaderNodeGroupBase *)get_node(type, id).get())->get_size();
            return true;
        } else if (what == "input_ports"_sv) {
            r_ret = ((VisualShaderNodeGroupBase *)get_node(type, id).get())->get_inputs();
            return true;
        } else if (what == "output_ports"_sv) {
            r_ret = ((VisualShaderNodeGroupBase *)get_node(type, id).get())->get_outputs();
            return true;
        } else if (what == "expression"_sv) {
            r_ret = ((VisualShaderNodeExpression *)get_node(type, id).get())->get_expression();
            return true;
        }
    }
    return false;
}
void VisualShader::_get_property_list(ListPOD<PropertyInfo> *p_list) const {

    //mode
    p_list->push_back(PropertyInfo(VariantType::INT, "mode", PropertyHint::Enum, "Spatial,CanvasItem,Particles"));
    //render modes

    Map<String, String> blend_mode_enums;
    Set<String> toggles;

    for (int i = 0; i < ShaderTypes::get_singleton()->get_modes(VS::ShaderMode(shader_mode)).size(); i++) {

        StringName mode = ShaderTypes::get_singleton()->get_modes(VS::ShaderMode(shader_mode))[i];
        int idx = 0;
        bool in_enum = false;
        while (render_mode_enums[idx].string) {
            if (StringUtils::begins_with(mode,render_mode_enums[idx].string)) {
                String begin(render_mode_enums[idx].string);
                String option = StringUtils::replace_first(mode,begin + '_', String());
                if (!blend_mode_enums.contains(begin)) {
                    blend_mode_enums[begin] = option;
                } else {
                    blend_mode_enums[begin] += "," + option;
                }
                in_enum = true;
                break;
            }
            idx++;
        }

        if (!in_enum) {
            toggles.insert(mode);
        }
    }

    for (eastl::pair<const String,String> &E : blend_mode_enums) {

        p_list->push_back(PropertyInfo(VariantType::INT, StringName("modes/" + E.first), PropertyHint::Enum, E.second));
    }

    for (const String &E : toggles) {
        p_list->push_back(PropertyInfo(VariantType::BOOL, StringName("flags/" + E)));
    }

    for (int i = 0; i < TYPE_MAX; i++) {
        for (auto &E : graph[i].nodes) {

            StringName prop_name("nodes/" + String(type_string[i])+"/" + itos(E.first));

            if (E.first != NODE_ID_OUTPUT) {

                p_list->push_back(PropertyInfo(VariantType::OBJECT, prop_name + "/node", PropertyHint::ResourceType, "VisualShaderNode", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_DO_NOT_SHARE_ON_DUPLICATE));
            }
            p_list->push_back(PropertyInfo(VariantType::VECTOR2, prop_name + "/position", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR));

            if (object_cast<VisualShaderNodeGroupBase>(E.second.node.get()) != nullptr) {
                p_list->push_back(PropertyInfo(VariantType::VECTOR2, prop_name + "/size", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR));
                p_list->push_back(PropertyInfo(VariantType::STRING, prop_name + "/input_ports", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR));
                p_list->push_back(PropertyInfo(VariantType::STRING, prop_name + "/output_ports", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR));
            }
            if (object_cast<VisualShaderNodeExpression>(E.second.node.get()) != nullptr) {
                p_list->push_back(PropertyInfo(VariantType::STRING, prop_name + "/expression", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR));
            }
        }
        p_list->push_back(PropertyInfo(VariantType::POOL_INT_ARRAY, StringName("nodes/") + type_string[i] + "/connections", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR));
    }
}

Error VisualShader::_write_node(Type type, StringBuilder &global_code, StringBuilder &global_code_per_node, Map<Type, StringBuilder> &global_code_per_func, StringBuilder &code, Vector<VisualShader::DefaultTextureParam> &def_tex_params, const VMap<ConnectionKey, const List<Connection>::Element *> &input_connections, const VMap<ConnectionKey, const List<Connection>::Element *> &output_connections, int node, Set<int> &processed, bool for_preview, Set<StringName> &r_classes) const {

    const Ref<VisualShaderNode> vsnode = graph[type].nodes.at(node).node;

    //check inputs recursively first
    int input_count = vsnode->get_input_port_count();
    for (int i = 0; i < input_count; i++) {
        ConnectionKey ck;
        ck.node = node;
        ck.port = i;

        if (input_connections.has(ck)) {
            int from_node = input_connections[ck]->deref().from_node;
            if (processed.contains(from_node)) {
                continue;
            }

            Error err = _write_node(type, global_code, global_code_per_node, global_code_per_func, code, def_tex_params, input_connections, output_connections, from_node, processed, for_preview, r_classes);
            if (err)
                return err;
        }
    }

    // then this node

    code += String("// ") + vsnode->get_caption() + ":" + itos(node) + "\n";
    Vector<String> input_vars;

    input_vars.resize(vsnode->get_input_port_count());
    String *inputs = input_vars.ptrw();

    for (int i = 0; i < input_count; i++) {
        ConnectionKey ck;
        ck.node = node;
        ck.port = i;

        if (input_connections.has(ck)) {
            //connected to something, use that output
            int from_node = input_connections[ck]->deref().from_node;
            int from_port = input_connections[ck]->deref().from_port;

            VisualShaderNode::PortType in_type = vsnode->get_input_port_type(i);
            VisualShaderNode::PortType out_type = graph[type].nodes.at(from_node).node->get_output_port_type(from_port);

            String src_var = "n_out" + itos(from_node) + "p" + itos(from_port);

            if (in_type == VisualShaderNode::PORT_TYPE_SAMPLER && out_type == VisualShaderNode::PORT_TYPE_SAMPLER) {
                VisualShaderNode *ptr = const_cast<VisualShaderNode *>(graph[type].nodes.at(from_node).node.get());
                if (ptr->has_method("get_input_real_name")) {
                    inputs[i] = ptr->call("get_input_real_name").as<String>();
                } else if (ptr->has_method("get_uniform_name")) {
                    inputs[i] = ptr->call("get_uniform_name").as<String>();
                } else {
                    inputs[i] = "";
                }
            } else if (in_type == out_type) {
                inputs[i] = src_var;
            } else if (in_type == VisualShaderNode::PORT_TYPE_SCALAR && out_type == VisualShaderNode::PORT_TYPE_VECTOR) {
                inputs[i] = "dot(" + src_var + ",vec3(0.333333,0.333333,0.333333))";
            } else if (in_type == VisualShaderNode::PORT_TYPE_VECTOR && out_type == VisualShaderNode::PORT_TYPE_SCALAR) {
                inputs[i] = "vec3(" + src_var + ")";
            } else if (in_type == VisualShaderNode::PORT_TYPE_BOOLEAN && out_type == VisualShaderNode::PORT_TYPE_VECTOR) {
                inputs[i] = "all(bvec3(" + src_var + "))";
            } else if (in_type == VisualShaderNode::PORT_TYPE_BOOLEAN && out_type == VisualShaderNode::PORT_TYPE_SCALAR) {
                inputs[i] = src_var + ">0.0?true:false";
            } else if (in_type == VisualShaderNode::PORT_TYPE_SCALAR && out_type == VisualShaderNode::PORT_TYPE_BOOLEAN) {
                inputs[i] = src_var + "?1.0:0.0";
            } else if (in_type == VisualShaderNode::PORT_TYPE_VECTOR && out_type == VisualShaderNode::PORT_TYPE_BOOLEAN) {
                inputs[i] = "vec3(" + src_var + "?1.0:0.0)";
            }
        } else {

            Variant defval = vsnode->get_input_port_default_value(i);
            if (defval.get_type() == VariantType::REAL || defval.get_type() == VariantType::INT) {
                float val = defval;
                inputs[i] = "n_in" + itos(node) + "p" + itos(i);
                code += "\tfloat " + inputs[i] + " = " + FormatVE("%.5f", val) + ";\n";
            } else if (defval.get_type() == VariantType::BOOL) {
                bool val = defval;
                inputs[i] = "n_in" + itos(node) + "p" + itos(i);
                code += "\tbool " + inputs[i] + " = " + (val ? "true" : "false") + ";\n";
            } else if (defval.get_type() == VariantType::VECTOR3) {
                Vector3 val = defval;
                inputs[i] = "n_in" + itos(node) + "p" + itos(i);
                code += "\tvec3 " + inputs[i] + " = " + FormatVE("vec3(%.5f,%.5f,%.5f);\n", val.x, val.y, val.z);
            } else if (defval.get_type() == VariantType::TRANSFORM) {
                Transform val = defval;
                val.basis.transpose();
                inputs[i] = "n_in" + itos(node) + "p" + itos(i);
                code+="\tmat4 " + inputs[i] + " = mat4( ";
                Array values;
                for (int j = 0; j < 3; j++) {
                    code += FormatVE("vec4(%.5f,%.5f,%.5f,0.0),",val.basis[j].x,val.basis[j].y,val.basis[j].z);
                }
                code += FormatVE("vec4(%.5f,%.5f,%.5f,0.0) );\n",val.origin.x,val.origin.y,val.origin.z);
            } else {
                //will go empty, node is expected to know what it is doing at this point and handle it
            }
        }
    }

    int output_count = vsnode->get_output_port_count();
    Vector<String> output_vars;
    output_vars.resize(vsnode->get_output_port_count());
    String *outputs = output_vars.ptrw();

    for (int i = 0; i < output_count; i++) {

        outputs[i] = "n_out" + itos(node) + "p" + itos(i);
        switch (vsnode->get_output_port_type(i)) {
            case VisualShaderNode::PORT_TYPE_SCALAR: code += String() + "\tfloat " + outputs[i] + ";\n"; break;
            case VisualShaderNode::PORT_TYPE_VECTOR: code += String() + "\tvec3 " + outputs[i] + ";\n"; break;
            case VisualShaderNode::PORT_TYPE_BOOLEAN: code += String() + "\tbool " + outputs[i] + ";\n"; break;
            case VisualShaderNode::PORT_TYPE_TRANSFORM: code += String() + "\tmat4 " + outputs[i] + ";\n"; break;
            default: {
            }
        }
    }

    Vector<VisualShader::DefaultTextureParam> params = vsnode->get_default_texture_parameters(type, node);
    for (int i = 0; i < params.size(); i++) {
        def_tex_params.push_back(params[i]);
    }

    Ref<VisualShaderNodeInput> input = dynamic_ref_cast<VisualShaderNodeInput>(vsnode);
    bool skip_global = input && for_preview;

    if (!skip_global) {

        global_code += vsnode->generate_global(get_mode(), type, node);

        StringName class_name = vsnode->get_class_name();
        if (class_name == "VisualShaderNodeCustom") {
            class_name = vsnode->get_script_instance()->get_script()->get_language()->get_global_class_name(vsnode->get_script_instance()->get_script()->get_path());
        }
        if (!r_classes.contains(class_name)) {
            global_code_per_node += vsnode->generate_global_per_node(get_mode(), type, node);
            for (int i = 0; i < TYPE_MAX; i++) {
                global_code_per_func[Type(i)] += vsnode->generate_global_per_func(get_mode(), Type(i), node);
            }
            r_classes.insert(class_name);
        }
    }

    code += vsnode->generate_code(get_mode(), type, node, inputs, outputs, for_preview);

    code += "\n"; //
    processed.insert(node);

    return OK;
}

void VisualShader::_update_shader() const {
    if (!dirty)
        return;

    dirty = false;

    StringBuilder global_code;
    StringBuilder global_code_per_node;
    Map<Type, StringBuilder> global_code_per_func;
    StringBuilder code;
    Vector<VisualShader::DefaultTextureParam> default_tex_params;
    Set<StringName> classes;
    List<int> insertion_pos;
    static constexpr const char *shader_mode_str[] = { "spatial", "canvas_item", "particles" };

    global_code += String() + "shader_type " + shader_mode_str[int(shader_mode)] + ";\n";

    String render_mode;

    {
        //fill render mode enums
        int idx = 0;
        while (render_mode_enums[idx].string) {

            if (shader_mode == render_mode_enums[idx].mode) {
                auto iter = modes.find_as(render_mode_enums[idx].string);
                if (modes.end()!=iter) {

                    int which = iter->second;
                    int count = 0;
                    for (int i = 0; i < ShaderTypes::get_singleton()->get_modes(VS::ShaderMode(shader_mode)).size(); i++) {
                        StringName mode = ShaderTypes::get_singleton()->get_modes(VS::ShaderMode(shader_mode))[i];
                        if (StringUtils::begins_with(mode,render_mode_enums[idx].string)) {
                            if (count == which) {
                                if (!render_mode.empty()) {
                                    render_mode += String(", ");
                                }
                                render_mode += mode;
                                break;
                            }
                            count++;
                        }
                    }
                }
            }
            idx++;
        }

        //fill render mode flags
        for (int i = 0; i < ShaderTypes::get_singleton()->get_modes(VS::ShaderMode(shader_mode)).size(); i++) {

            StringName mode = ShaderTypes::get_singleton()->get_modes(VS::ShaderMode(shader_mode))[i];
            if (flags.contains(mode)) {
                if (!render_mode.empty()) {
                    render_mode += String(", ");
                }
                render_mode += mode;
            }
        }
    }

    if (!render_mode.empty()) {

        global_code += "render_mode " + render_mode + ";\n\n";
    }

    static const char *func_name[TYPE_MAX] = { "vertex", "fragment", "light" };

    String global_expressions;
    for (int i = 0, index = 0; i < TYPE_MAX; i++) {
        for (auto &E : graph[i].nodes) {
            Ref<VisualShaderNodeGlobalExpression> global_expression(dynamic_ref_cast<VisualShaderNodeGlobalExpression>(E.second.node));
            if (global_expression) {

                String expr;
                expr += "// " + global_expression->get_caption() + ":" + itos(index++) + "\n";
                expr += global_expression->generate_global(get_mode(), Type(i), -1);
                expr =StringUtils::replace(expr,"\n", "\n\t");
                expr += '\n';
                global_expressions += expr;
            }
        }
    }

    for (int i = 0; i < TYPE_MAX; i++) {

        //make it faster to go around through shader
        VMap<ConnectionKey, const List<Connection>::Element *> input_connections;
        VMap<ConnectionKey, const List<Connection>::Element *> output_connections;

        for (const List<Connection>::Element *E = graph[i].connections.front(); E; E = E->next()) {
            ConnectionKey from_key;
            from_key.node = E->deref().from_node;
            from_key.port = E->deref().from_port;

            output_connections.insert(from_key, E);

            ConnectionKey to_key;
            to_key.node = E->deref().to_node;
            to_key.port = E->deref().to_port;

            input_connections.insert(to_key, E);
        }

        code += "\nvoid " + String(func_name[i]) + "() {\n";

        Set<int> processed;
        Error err = _write_node(Type(i), global_code, global_code_per_node, global_code_per_func, code, default_tex_params, input_connections, output_connections, NODE_ID_OUTPUT, processed, false, classes);
        ERR_FAIL_COND(err != OK)
        insertion_pos.push_back(code.get_string_length());

        code += "}\n";
    }

    //set code secretly
    global_code += "\n\n";
    String final_code = global_code;
    final_code += global_code_per_node;
    final_code += global_expressions;
    String tcode = code;
    for (int i = 0; i < TYPE_MAX; i++) {
        tcode = StringUtils::insert(tcode,insertion_pos[i], global_code_per_func[Type(i)].as_string());
    }
    final_code += tcode;

    const_cast<VisualShader *>(this)->set_code(final_code);
    for (int i = 0; i < default_tex_params.size(); i++) {
        const_cast<VisualShader *>(this)->set_default_texture_param(default_tex_params[i].name, default_tex_params[i].param);
    }
    if (previous_code != final_code) {
        const_cast<VisualShader *>(this)->emit_signal("changed");
    }
    previous_code = final_code;
}

void VisualShader::_queue_update() {
    if (dirty) {
        return;
    }

    dirty = true;
    call_deferred("_update_shader");
}

void VisualShader::_input_type_changed(Type p_type, int p_id) {
    //erase connections using this input, as type changed
    Graph *g = &graph[p_type];

    for (List<Connection>::Element *E = g->connections.front(); E;) {
        List<Connection>::Element *N = E->next();
        if (E->deref().from_node == p_id) {
            g->connections.erase(E);
        }
        E = N;
    }
}

void VisualShader::rebuild() {
    dirty = true;
    _update_shader();
}

void VisualShader::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_mode", {"mode"}), &VisualShader::set_mode);

    MethodBinder::bind_method(D_METHOD("add_node", {"type", "node", "position", "id"}), &VisualShader::add_node);
    MethodBinder::bind_method(D_METHOD("get_node", {"type", "id"}), &VisualShader::get_node);

    MethodBinder::bind_method(D_METHOD("set_node_position", {"type", "id", "position"}), &VisualShader::set_node_position);
    MethodBinder::bind_method(D_METHOD("get_node_position", {"type", "id"}), &VisualShader::get_node_position);

    MethodBinder::bind_method(D_METHOD("get_node_list", {"type"}), &VisualShader::get_node_list);
    MethodBinder::bind_method(D_METHOD("get_valid_node_id", {"type"}), &VisualShader::get_valid_node_id);

    MethodBinder::bind_method(D_METHOD("remove_node", {"type", "id"}), &VisualShader::remove_node);

    MethodBinder::bind_method(D_METHOD("is_node_connection", {"type", "from_node", "from_port", "to_node", "to_port"}), &VisualShader::is_node_connection);
    MethodBinder::bind_method(D_METHOD("can_connect_nodes", {"type", "from_node", "from_port", "to_node", "to_port"}), &VisualShader::is_node_connection);

    MethodBinder::bind_method(D_METHOD("connect_nodes", {"type", "from_node", "from_port", "to_node", "to_port"}), &VisualShader::connect_nodes);
    MethodBinder::bind_method(D_METHOD("disconnect_nodes", {"type", "from_node", "from_port", "to_node", "to_port"}), &VisualShader::disconnect_nodes);
    MethodBinder::bind_method(D_METHOD("connect_nodes_forced", {"type", "from_node", "from_port", "to_node", "to_port"}), &VisualShader::connect_nodes_forced);

    MethodBinder::bind_method(D_METHOD("get_node_connections", {"type"}), &VisualShader::_get_node_connections);

    MethodBinder::bind_method(D_METHOD("set_graph_offset", {"offset"}), &VisualShader::set_graph_offset);
    MethodBinder::bind_method(D_METHOD("get_graph_offset"), &VisualShader::get_graph_offset);

    MethodBinder::bind_method(D_METHOD("_queue_update"), &VisualShader::_queue_update);
    MethodBinder::bind_method(D_METHOD("_update_shader"), &VisualShader::_update_shader);

    MethodBinder::bind_method(D_METHOD("_input_type_changed"), &VisualShader::_input_type_changed);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "graph_offset", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "set_graph_offset", "get_graph_offset");

    BIND_ENUM_CONSTANT(TYPE_VERTEX)
    BIND_ENUM_CONSTANT(TYPE_FRAGMENT)
    BIND_ENUM_CONSTANT(TYPE_LIGHT)
    BIND_ENUM_CONSTANT(TYPE_MAX)

    BIND_CONSTANT(NODE_ID_INVALID);
    BIND_CONSTANT(NODE_ID_OUTPUT);
}

VisualShader::VisualShader() {
    shader_mode = ShaderMode::SPATIAL;

    for (int i = 0; i < TYPE_MAX; i++) {
        Ref<VisualShaderNodeOutput> output(make_ref_counted<VisualShaderNodeOutput>());
        output->shader_type = Type(i);
        output->shader_mode = shader_mode;
        graph[i].nodes[NODE_ID_OUTPUT].node = output;
        graph[i].nodes[NODE_ID_OUTPUT].position = Vector2(400, 150);
    }

    dirty = true;
}

///////////////////////////////////////////////////////////

const VisualShaderNodeInput::Port VisualShaderNodeInput::ports[] = {
    // Spatial, Vertex
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "vertex", "VERTEX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "normal", "NORMAL" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "tangent", "TANGENT" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "binormal", "BINORMAL" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "uv", "vec3(UV,0.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "uv2", "vec3(UV2,0.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "color", "COLOR.rgb" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "COLOR.a" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "point_size", "POINT_SIZE" },

    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "world", "WORLD_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "modelview", "MODELVIEW_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "camera", "CAMERA_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "inv_camera", "INV_CAMERA_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "projection", "PROJECTION_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "inv_projection", "INV_PROJECTION_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "viewport_size", "vec3(VIEWPORT_SIZE, 0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_BOOLEAN, "output_is_srgb", "OUTPUT_IS_SRGB" },

    // Spatial, Fragment
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "fragcoord", "FRAGCOORD.xyz" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "vertex", "VERTEX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "normal", "NORMAL" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "tangent", "TANGENT" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "binormal", "BINORMAL" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "view", "VIEW" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "uv", "vec3(UV,0.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "uv2", "vec3(UV2,0.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "color", "COLOR.rgb" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "COLOR.a" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "point_coord", "vec3(POINT_COORD,0.0)" },

    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "screen_uv", "vec3(SCREEN_UV,0.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "side", "float(FRONT_FACING ? 1.0 : 0.0)" },

    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_TRANSFORM, "world", "WORLD_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_TRANSFORM, "inv_camera", "INV_CAMERA_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_TRANSFORM, "camera", "CAMERA_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_TRANSFORM, "projection", "PROJECTION_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_TRANSFORM, "inv_projection", "INV_PROJECTION_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "viewport_size", "vec3(VIEWPORT_SIZE, 0.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_BOOLEAN, "output_is_srgb", "OUTPUT_IS_SRGB" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_BOOLEAN, "front_facing", "FRONT_FACING" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SAMPLER, "screen_texture", "SCREEN_TEXTURE" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SAMPLER, "depth_texture", "DEPTH_TEXTURE" },


    // Spatial, Light
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "fragcoord", "FRAGCOORD.xyz" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "normal", "NORMAL" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "view", "VIEW" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "light", "LIGHT" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "light_color", "LIGHT_COLOR" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "attenuation", "ATTENUATION" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "albedo", "ALBEDO" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "transmission", "TRANSMISSION" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "diffuse", "DIFFUSE_LIGHT" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "specular", "SPECULAR_LIGHT" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "roughness", "ROUGHNESS" },

    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_TRANSFORM, "world", "WORLD_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_TRANSFORM, "inv_camera", "INV_CAMERA_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_TRANSFORM, "camera", "CAMERA_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_TRANSFORM, "projection", "PROJECTION_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_TRANSFORM, "inv_projection", "INV_PROJECTION_MATRIX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "viewport_size", "vec3(VIEWPORT_SIZE, 0.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_BOOLEAN, "output_is_srgb", "OUTPUT_IS_SRGB" },
    // Canvas Item, Vertex
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "vertex", "vec3(VERTEX,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "uv", "vec3(UV,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "color", "COLOR.rgb" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "COLOR.a" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "point_size", "POINT_SIZE" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "texture_pixel_size", "vec3(TEXTURE_PIXEL_SIZE, 1.0)" },

    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "world", "WORLD_MATRIX" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "projection", "PROJECTION_MATRIX" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "extra", "EXTRA_MATRIX" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "light_pass", "float(AT_LIGHT_PASS ? 1.0 : 0.0)" },
    // Canvas Item, Fragment
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "fragcoord", "FRAGCOORD.xyz" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "uv", "vec3(UV,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "color", "COLOR.rgb" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "COLOR.a" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "screen_uv", "vec3(SCREEN_UV,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "texture_pixel_size", "vec3(TEXTURE_PIXEL_SIZE, 1.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "screen_pixel_size", "vec3(SCREEN_PIXEL_SIZE, 1.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "point_coord", "vec3(POINT_COORD,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "light_pass", "float(AT_LIGHT_PASS ? 1.0 : 0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SAMPLER, "texture", "TEXTURE" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SAMPLER, "normal_texture", "NORMAL_TEXTURE" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SAMPLER, "screen_texture", "SCREEN_TEXTURE" },

    // Canvas Item, Light
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "fragcoord", "FRAGCOORD.xyz" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "uv", "vec3(UV,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "normal", "NORMAL" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "color", "COLOR.rgb" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "COLOR.a" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "light_vec", "vec3(LIGHT_VEC,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "light_height", "LIGHT_HEIGHT" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "light_color", "LIGHT_COLOR.rgb" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "light_alpha", "LIGHT_COLOR.a" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "light_uv", "vec3(LIGHT_UV,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "shadow_color", "SHADOW_COLOR.rgb" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "screen_uv", "vec3(SCREEN_UV,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "texture_pixel_size", "vec3(TEXTURE_PIXEL_SIZE, 1.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "point_coord", "vec3(POINT_COORD,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SAMPLER, "texture", "TEXTURE" },

    // Particles, Vertex
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "color", "COLOR.rgb" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "COLOR.a" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "velocity", "VELOCITY" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "restart", "float(RESTART ? 1.0 : 0.0)" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "active", "float(ACTIVE ? 1.0 : 0.0)" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "custom", "CUSTOM.rgb" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "custom_alpha", "CUSTOM.a" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "transform", "TRANSFORM" },

    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "delta", "DELTA" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "lifetime", "LIFETIME" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "index", "float(INDEX)" },

    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "emission_transform", "EMISSION_TRANSFORM" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
    { ShaderMode::MAX, VisualShader::TYPE_MAX, VisualShaderNode::PORT_TYPE_TRANSFORM, nullptr, nullptr },
};

const VisualShaderNodeInput::Port VisualShaderNodeInput::preview_ports[] = {

    // Spatial, Fragment
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "normal", "vec3(0.0,0.0,1.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "tangent", "vec3(0.0,1.0,0.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "binormal", "vec3(1.0,0.0,0.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "uv", "vec3(UV,0.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "uv2", "vec3(UV,0.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "color", "vec3(1.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "1.0" },

    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "screen_uv", "vec3(SCREEN_UV,0.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "side", "1.0" },

    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "viewport_size", "vec3(1.0,1.0, 0.0)" },

    // Spatial, Light
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "normal", "vec3(0.0,0.0,1.0)" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "viewport_size", "vec3(1.0, 1.0, 0.0)" },
    // Canvas Item, Vertex
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "vertex", "vec3(VERTEX,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "uv", "vec3(UV,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "color", "vec3(1.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "1.0" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
    // Canvas Item, Fragment
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "uv", "vec3(UV,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "color", "vec3(1.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "1.0" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "screen_uv", "vec3(SCREEN_UV,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
    // Canvas Item, Light
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "uv", "vec3(UV,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "normal", "vec3(0.0,0.0,1.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "color", "vec3(1.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "1.0" },

    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "screen_uv", "vec3(SCREEN_UV,0.0)" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
    // Particles, Vertex
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "color", "vec3(1.0)" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "1.0" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "velocity", "vec3(0.0,0.0,1.0)" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
    { ShaderMode::MAX, VisualShader::TYPE_MAX, VisualShaderNode::PORT_TYPE_TRANSFORM, nullptr, nullptr },
};

int VisualShaderNodeInput::get_input_port_count() const {

    return 0;
}
VisualShaderNodeInput::PortType VisualShaderNodeInput::get_input_port_type(int p_port) const {

    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeInput::get_input_port_name(int p_port) const {

    return StringName();
}

int VisualShaderNodeInput::get_output_port_count() const {

    return 1;
}
VisualShaderNodeInput::PortType VisualShaderNodeInput::get_output_port_type(int p_port) const {

    return get_input_type_by_name(input_name);
}
StringName VisualShaderNodeInput::get_output_port_name(int p_port) const {
    return StringName();
}

se_string_view VisualShaderNodeInput::get_caption() const {
    return "Input";
}

String VisualShaderNodeInput::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    if (get_output_port_type(0) == PORT_TYPE_SAMPLER) {
        return "";
    }
    if (p_for_preview) {
        int idx = 0;

        String code;

        while (preview_ports[idx].mode != ShaderMode::MAX) {
            if (preview_ports[idx].mode == shader_mode && preview_ports[idx].shader_type == shader_type && preview_ports[idx].name == input_name) {
                code = "\t" + p_output_vars[0] + " = " + preview_ports[idx].string + ";\n";
                break;
            }
            idx++;
        }

        if (code.empty()) {
            switch (get_output_port_type(0)) {
                case PORT_TYPE_SCALAR: {
                    code = "\t" + p_output_vars[0] + " = 0.0;\n";
                } break; //default (none found) is scalar
                case PORT_TYPE_VECTOR: {
                    code = "\t" + p_output_vars[0] + " = vec3(0.0);\n";
                } break; //default (none found) is scalar
                case PORT_TYPE_TRANSFORM: {
                    code = "\t" + p_output_vars[0] + " = mat4( vec4(1.0,0.0,0.0,0.0), vec4(0.0,1.0,0.0,0.0), vec4(0.0,0.0,1.0,0.0), vec4(0.0,0.0,0.0,1.0) );\n";
                } break; //default (none found) is scalar
                case PORT_TYPE_BOOLEAN: {
                    code = "\t" + p_output_vars[0] + " = false;\n";
                } break;
                default:
                    break;
            }
        }

        return code;

    } else {
        int idx = 0;

        String code;

        while (ports[idx].mode != ShaderMode::MAX) {
            if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type && ports[idx].name == input_name) {
                code = "\t" + p_output_vars[0] + " = " + ports[idx].string + ";\n";
                break;
            }
            idx++;
        }

        if (code.empty()) {
            code = "\t" + p_output_vars[0] + " = 0.0;\n"; //default (none found) is scalar
        }

        return code;
    }
}

void VisualShaderNodeInput::set_input_name(StringName p_name) {
    PortType prev_type = get_input_type_by_name(input_name);
    input_name = p_name;
    emit_changed();
    if (get_input_type_by_name(input_name) != prev_type) {
        emit_signal("input_type_changed");
    }
}

StringName VisualShaderNodeInput::get_input_name() const {
    return input_name;
}
String VisualShaderNodeInput::get_input_real_name() const {

    int idx = 0;

    while (ports[idx].mode != ShaderMode::MAX) {
        if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type && ports[idx].name == input_name) {
            return String(ports[idx].string);
        }
        idx++;
    }

    return "";
}
VisualShaderNodeInput::PortType VisualShaderNodeInput::get_input_type_by_name(StringName p_name) const {

    int idx = 0;

    while (ports[idx].mode != ShaderMode::MAX) {
        if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type && ports[idx].name == p_name) {
            return ports[idx].type;
        }
        idx++;
    }

    return PORT_TYPE_SCALAR;
}

int VisualShaderNodeInput::get_input_index_count() const {
    int idx = 0;
    int count = 0;

    while (ports[idx].mode != ShaderMode::MAX) {
        if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
            count++;
        }
        idx++;
    }

    return count;
}

VisualShaderNodeInput::PortType VisualShaderNodeInput::get_input_index_type(int p_index) const {
    int idx = 0;
    int count = 0;

    while (ports[idx].mode != ShaderMode::MAX) {
        if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
            if (count == p_index) {
                return ports[idx].type;
            }
            count++;
        }
        idx++;
    }

    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeInput::get_input_index_name(int p_index) const {
    int idx = 0;
    int count = 0;

    while (ports[idx].mode != ShaderMode::MAX) {
        if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
            if (count == p_index) {
                return StringName(ports[idx].name);
            }
            count++;
        }
        idx++;
    }

    return StringName();
}

void VisualShaderNodeInput::_validate_property(PropertyInfo &property) const {

    if (property.name == "input_name") {
        String port_list;

        int idx = 0;

        while (ports[idx].mode != ShaderMode::MAX) {
            if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
                if (!port_list.empty()) {
                    port_list += ',';
                }
                port_list += ports[idx].name;
            }
            idx++;
        }

        if (port_list.empty()) {
            port_list = TTR("None");
        }
        property.hint_string = port_list;
    }
}

Vector<StringName> VisualShaderNodeInput::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("input_name");
    return props;
}

void VisualShaderNodeInput::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_input_name", {"name"}), &VisualShaderNodeInput::set_input_name);
    MethodBinder::bind_method(D_METHOD("get_input_name"), &VisualShaderNodeInput::get_input_name);
    MethodBinder::bind_method(D_METHOD("get_input_real_name"), &VisualShaderNodeInput::get_input_real_name);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "input_name", PropertyHint::Enum, ""), "set_input_name", "get_input_name");
    ADD_SIGNAL(MethodInfo("input_type_changed"));
}
VisualShaderNodeInput::VisualShaderNodeInput() {
    input_name = "[None]";
    // changed when set
    shader_type = VisualShader::TYPE_MAX;
    shader_mode = ShaderMode::MAX;
}

////////////////////////////////////////////

const VisualShaderNodeOutput::Port VisualShaderNodeOutput::ports[] = {
    // Spatial, Vertex
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "vertex", "VERTEX" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "normal", "NORMAL" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "tangent", "TANGENT" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "binormal", "BINORMAL" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "uv", "UV:xy" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "uv2", "UV2:xy" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "color", "COLOR.rgb" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "COLOR.a" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "roughness", "ROUGHNESS" },
    // Spatial, Fragment

    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "albedo", "ALBEDO" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "ALPHA" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "metallic", "METALLIC" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "roughness", "ROUGHNESS" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "specular", "SPECULAR" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "emission", "EMISSION" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "ao", "AO" },

    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "normal", "NORMAL" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "normalmap", "NORMALMAP" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "normalmap_depth", "NORMALMAP_DEPTH" },

    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "rim", "RIM" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "rim_tint", "RIM_TINT" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "clearcoat", "CLEARCOAT" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "clearcoat_gloss", "CLEARCOAT_GLOSS" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "anisotropy", "ANISOTROPY" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "anisotropy_flow", "ANISOTROPY_FLOW:xy" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "subsurf_scatter", "SSS_STRENGTH" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "transmission", "TRANSMISSION" },

    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "alpha_scissor", "ALPHA_SCISSOR" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "ao_light_affect", "AO_LIGHT_AFFECT" },

    // Spatial, Light
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "diffuse", "DIFFUSE_LIGHT" },
    { ShaderMode::SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "specular", "SPECULAR_LIGHT" },
    // Canvas Item, Vertex
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "vertex", "VERTEX:xy" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "uv", "UV:xy" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "color", "COLOR.rgb" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "COLOR.a" },
    // Canvas Item, Fragment
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "color", "COLOR.rgb" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "COLOR.a" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "normal", "NORMAL" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR, "normalmap", "NORMALMAP" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "normalmap_depth", "NORMALMAP_DEPTH" },
    // Canvas Item, Light
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR, "light", "LIGHT.rgb" },
    { ShaderMode::CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "light_alpha", "LIGHT.rgb" },
    // Particles, Vertex
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "color", "COLOR.rgb" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "alpha", "COLOR.a" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "velocity", "VELOCITY" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR, "custom", "CUSTOM.rgb" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "custom_alpha", "CUSTOM.a" },
    { ShaderMode::PARTICLES, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "transform", "TRANSFORM" },
    { ShaderMode::MAX, VisualShader::TYPE_MAX, VisualShaderNode::PORT_TYPE_TRANSFORM, nullptr, nullptr },
};

int VisualShaderNodeOutput::get_input_port_count() const {

    int idx = 0;
    int count = 0;

    while (ports[idx].mode != ShaderMode::MAX) {
        if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
            count++;
        }
        idx++;
    }

    return count;
}

VisualShaderNodeOutput::PortType VisualShaderNodeOutput::get_input_port_type(int p_port) const {

    int idx = 0;
    int count = 0;

    while (ports[idx].mode != ShaderMode::MAX) {
        if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
            if (count == p_port) {
                return ports[idx].type;
            }
            count++;
        }
        idx++;
    }

    return PORT_TYPE_SCALAR;
}

StringName VisualShaderNodeOutput::get_input_port_name(int p_port) const {

    int idx = 0;
    int count = 0;

    while (ports[idx].mode != ShaderMode::MAX) {
        if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
            if (count == p_port) {
                return StringName(StringUtils::capitalize(String(ports[idx].name)));
            }
            count++;
        }
        idx++;
    }

    return StringName();
}

Variant VisualShaderNodeOutput::get_input_port_default_value(int p_port) const {
    return Variant();
}

int VisualShaderNodeOutput::get_output_port_count() const {

    return 0;
}
VisualShaderNodeOutput::PortType VisualShaderNodeOutput::get_output_port_type(int p_port) const {
    return PORT_TYPE_SCALAR;
}
StringName VisualShaderNodeOutput::get_output_port_name(int p_port) const {
    return StringName();
}

bool VisualShaderNodeOutput::is_port_separator(int p_index) const {

    if (shader_mode == ShaderMode::SPATIAL && shader_type == VisualShader::TYPE_FRAGMENT) {
        StringName name = get_input_port_name(p_index);
        return (name == "Normal" || name == "Rim" || name == "Alpha Scissor");
    }
    return false;
}

se_string_view VisualShaderNodeOutput::get_caption() const {
    return "Output";
}

String VisualShaderNodeOutput::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    int idx = 0;
    int count = 0;

    String code;
    while (ports[idx].mode != ShaderMode::MAX) {
        if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {

            if (!p_input_vars[count].empty()) {
                String s(ports[idx].string);
                if (StringUtils::contains(s,':')) {
                    code += "\t" + StringUtils::get_slice(s,':', 0) + " = " + p_input_vars[count] + "." + StringUtils::get_slice(s,':', 1) + ";\n";
                } else {
                    code += "\t" + s + " = " + p_input_vars[count] + ";\n";
                }
            }
            count++;
        }
        idx++;
    }

    return code;
}

VisualShaderNodeOutput::VisualShaderNodeOutput() {
}

///////////////////////////

void VisualShaderNodeUniform::set_uniform_name(const StringName &p_name) {
    uniform_name = p_name;
    emit_signal("name_changed");
    emit_changed();
}

StringName VisualShaderNodeUniform::get_uniform_name() const {
    return uniform_name;
}

void VisualShaderNodeUniform::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_uniform_name", {"name"}), &VisualShaderNodeUniform::set_uniform_name);
    MethodBinder::bind_method(D_METHOD("get_uniform_name"), &VisualShaderNodeUniform::get_uniform_name);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "uniform_name"), "set_uniform_name", "get_uniform_name");
}

VisualShaderNodeUniform::VisualShaderNodeUniform() {
}

////////////// GroupBase

se_string_view VisualShaderNodeGroupBase::get_caption() const {
    return "Group";
}

void VisualShaderNodeGroupBase::set_size(const Vector2 &p_size) {
    size = p_size;
}

Vector2 VisualShaderNodeGroupBase::get_size() const {
    return size;
}

void VisualShaderNodeGroupBase::set_inputs(const String &p_inputs) {

    if (inputs == p_inputs)
        return;

    clear_input_ports();

    inputs = p_inputs;

    PODVector<se_string_view> input_strings = StringUtils::split(inputs,';', false);

    int input_port_count = input_strings.size();

    for (int i = 0; i < input_port_count; i++) {

        PODVector<se_string_view> arr = StringUtils::split(input_strings[i],',');
        ERR_FAIL_COND(arr.size() != 3)

        int port_idx = StringUtils::to_int(arr[0]);
        int port_type = StringUtils::to_int(arr[1]);
        se_string_view port_name(arr[2]);

        Port port;
        port.type = (PortType)port_type;
        port.name = port_name;
        input_ports[port_idx] = port;
    }
}

String VisualShaderNodeGroupBase::get_inputs() const {
    return inputs;
}

void VisualShaderNodeGroupBase::set_outputs(const String &p_outputs) {

    if (outputs == p_outputs)
        return;

    clear_output_ports();

    outputs = p_outputs;

    PODVector<se_string_view> output_strings = StringUtils::split(outputs,';', false);

    int output_port_count = output_strings.size();

    for (int i = 0; i < output_port_count; i++) {

        PODVector<se_string_view> arr = StringUtils::split(output_strings[i],',');
        ERR_FAIL_COND(arr.size() != 3)

        int port_idx = StringUtils::to_int(arr[0]);
        int port_type = StringUtils::to_int(arr[1]);
        se_string_view port_name = arr[2];

        Port port;
        port.type = (PortType)port_type;
        port.name = port_name;
        output_ports[port_idx] = port;
    }
}

String VisualShaderNodeGroupBase::get_outputs() const {
    return outputs;
}

bool VisualShaderNodeGroupBase::is_valid_port_name(const String &p_name) const {
    if (!StringUtils::is_valid_identifier(p_name)) {
        return false;
    }
    for (int i = 0; i < get_input_port_count(); i++) {
        if (get_input_port_name(i) == p_name) {
            return false;
        }
    }
    for (int i = 0; i < get_output_port_count(); i++) {
        if (get_output_port_name(i) == p_name) {
            return false;
        }
    }
    return true;
}

void VisualShaderNodeGroupBase::add_input_port(int p_id, int p_type, const String &p_name) {

    String str = itos(p_id) + "," + itos(p_type) + "," + p_name + ";";
    PODVector<se_string_view> inputs_strings = StringUtils::split(inputs,';', false);
    int index = 0;
    if (p_id < inputs_strings.size()) {
        for (int i = 0; i < inputs_strings.size(); i++) {
            if (i == p_id) {
                inputs = StringUtils::insert(inputs,index, str);
                break;
            }
            index += inputs_strings[i].size();
        }
    } else {
        inputs += str;
    }

    inputs_strings = StringUtils::split(inputs,';', false);
    index = 0;

    for (size_t i = 0; i < inputs_strings.size(); i++) {
        int count = 0;
        for (size_t j = 0; j < inputs_strings[i].size(); j++) {
            if (inputs_strings[i][j] == ',') {
                break;
            }
            count++;
        }

        StringUtils::erase(inputs,index, count);
        inputs = StringUtils::insert(inputs,index, itos(i));
        index += inputs_strings[i].size();
    }

    _apply_port_changes();
}

void VisualShaderNodeGroupBase::remove_input_port(int p_id) {

    ERR_FAIL_COND(!has_input_port(p_id))

    PODVector<se_string_view> inputs_strings = StringUtils::split(inputs,';', false);
    int count = 0;
    int index = 0;
    for (int i = 0; i < inputs_strings.size(); i++) {
        PODVector<se_string_view> arr = StringUtils::split(inputs_strings[i],',');
        if (StringUtils::to_int(arr[0]) == p_id) {
            count = inputs_strings[i].size();
            break;
        }
        index += inputs_strings[i].size();
    }
    StringUtils::erase(inputs,index, count);

    inputs_strings = StringUtils::split(inputs,';', false);
    for (int i = p_id; i < inputs_strings.size(); i++) {
        inputs = StringUtils::replace_first(inputs,StringUtils::split(inputs_strings[i],',')[0], itos(i));
    }

    _apply_port_changes();
}

int VisualShaderNodeGroupBase::get_input_port_count() const {
    return input_ports.size();
}

bool VisualShaderNodeGroupBase::has_input_port(int p_id) const {
    return input_ports.contains(p_id);
}

void VisualShaderNodeGroupBase::add_output_port(int p_id, int p_type, const String &p_name) {

    FixedVector<se_string_view,16,true> outputs_strings;
    String str = itos(p_id) + "," + itos(p_type) + "," + p_name + ";";
    String::split_ref(outputs_strings,outputs,';');
    int index = 0;
    if (p_id < outputs_strings.size()) {
        for (size_t i = 0; i < outputs_strings.size(); i++) {
            if (i == p_id) {
                outputs = StringUtils::insert(outputs,index, str);
                break;
            }
            index += outputs_strings[i].size();
        }
    } else {
        outputs += str;
    }
    outputs_strings.clear();
    String::split_ref(outputs_strings,outputs,';');
    index = 0;

    for (size_t i = 0; i < outputs_strings.size(); i++) {
        int count = 0;
        for (size_t j = 0; j < outputs_strings[i].size(); j++) {
            if (outputs_strings[i][j] == ',') {
                break;
            }
            count++;
        }

        StringUtils::erase(outputs,index, count);
        outputs = StringUtils::insert(outputs,index, itos(i));
        index += outputs_strings[i].size();
    }

    _apply_port_changes();
}

void VisualShaderNodeGroupBase::remove_output_port(int p_id) {

    ERR_FAIL_COND(!has_output_port(p_id))

    PODVector<se_string_view> outputs_strings = StringUtils::split(outputs,';', false);
    int count = 0;
    int index = 0;
    for (se_string_view output : outputs_strings) {
        PODVector<se_string_view> arr = StringUtils::split(output,',');
        if (StringUtils::to_int(arr[0]) == p_id) {
            count = output.size();
            break;
        }
        index += output.size();
    }
    StringUtils::erase(outputs,index, count);

    outputs_strings = StringUtils::split(outputs,';', false);
    for (int i = p_id; i < outputs_strings.size(); i++) {
        outputs = StringUtils::replace_first(outputs,StringUtils::split(outputs_strings[i],',')[0], itos(i));
    }

    _apply_port_changes();
}

int VisualShaderNodeGroupBase::get_output_port_count() const {
    return output_ports.size();
}

bool VisualShaderNodeGroupBase::has_output_port(int p_id) const {
    return output_ports.contains(p_id);
}

void VisualShaderNodeGroupBase::clear_input_ports() {
    input_ports.clear();
}

void VisualShaderNodeGroupBase::clear_output_ports() {
    output_ports.clear();
}

void VisualShaderNodeGroupBase::set_input_port_type(int p_id, int p_type) {

    ERR_FAIL_COND(!has_input_port(p_id))
    ERR_FAIL_COND(p_type < 0 || p_type >= PORT_TYPE_MAX)

    if (input_ports[p_id].type == p_type)
        return;

    PODVector<se_string_view> inputs_strings = StringUtils::split(inputs,';', false);
    int count = 0;
    int index = 0;
    for (int i = 0; i < inputs_strings.size(); i++) {
        PODVector<se_string_view> arr = StringUtils::split(inputs_strings[i],',');
        ERR_FAIL_COND(arr.size() != 3)
        if (StringUtils::to_int(arr[0]) == p_id) {
            index += arr[0].size();
            count = arr[1].size() - 1;
            break;
        }
        index += inputs_strings[i].size();
    }

    StringUtils::erase(inputs,index, count);

    inputs = StringUtils::insert(inputs,index, StringUtils::num(p_type));

    _apply_port_changes();
}

VisualShaderNodeGroupBase::PortType VisualShaderNodeGroupBase::get_input_port_type(int p_id) const {
    ERR_FAIL_COND_V(!input_ports.contains(p_id), (PortType)0)
    return input_ports.at(p_id).type;
}

void VisualShaderNodeGroupBase::set_input_port_name(int p_id, const String &p_name) {

    ERR_FAIL_COND(!has_input_port(p_id))
    ERR_FAIL_COND(!is_valid_port_name(p_name))

    if (input_ports[p_id].name == p_name)
        return;

    PODVector<se_string_view> inputs_strings = StringUtils::split(inputs,';', false);
    int count = 0;
    int index = 0;
    for (int i = 0; i < inputs_strings.size(); i++) {
        PODVector<se_string_view> arr = StringUtils::split(inputs_strings[i],',');
        if (StringUtils::to_int(arr[0]) == p_id) {
            index += arr[0].size() + arr[1].size();
            count = arr[2].size() - 1;
            break;
        }
        index += inputs_strings[i].size();
    }

    StringUtils::erase(inputs,index, count);

    inputs = StringUtils::insert(inputs,index, p_name);

    _apply_port_changes();
}

StringName VisualShaderNodeGroupBase::get_input_port_name(int p_id) const {
    ERR_FAIL_COND_V(!input_ports.contains(p_id), StringName())
    return StringName(input_ports.at(p_id).name);
}

void VisualShaderNodeGroupBase::set_output_port_type(int p_id, int p_type) {

    ERR_FAIL_COND(!has_output_port(p_id))
    ERR_FAIL_COND(p_type < 0 || p_type >= PORT_TYPE_MAX)

    if (output_ports[p_id].type == p_type)
        return;

    PODVector<se_string_view> output_strings = StringUtils::split(outputs,';', false);
    int count = 0;
    int index = 0;
    for (int i = 0; i < output_strings.size(); i++) {
        PODVector<se_string_view> arr = StringUtils::split(output_strings[i],',');
        if (StringUtils::to_int(arr[0]) == p_id) {
            index += arr[0].size();
            count = arr[1].size() - 1;
            break;
        }
        index += output_strings[i].size();
    }

    StringUtils::erase(outputs,index, count);

    outputs = StringUtils::insert(outputs,index, itos(p_type));

    _apply_port_changes();
}

VisualShaderNodeGroupBase::PortType VisualShaderNodeGroupBase::get_output_port_type(int p_id) const {
    ERR_FAIL_COND_V(!output_ports.contains(p_id), (PortType)0)
    return output_ports.at(p_id).type;
}

void VisualShaderNodeGroupBase::set_output_port_name(int p_id, const String &p_name) {

    ERR_FAIL_COND(!has_output_port(p_id))
    ERR_FAIL_COND(!is_valid_port_name(p_name))

    if (output_ports[p_id].name == p_name)
        return;

    PODVector<se_string_view> output_strings = StringUtils::split(outputs,';', false);
    int count = 0;
    int index = 0;
    for (int i = 0; i < output_strings.size(); i++) {
        PODVector<se_string_view> arr = StringUtils::split(output_strings[i],',');
        if (StringUtils::to_int(arr[0]) == p_id) {
            index += arr[0].size() + arr[1].size();
            count = arr[2].size() - 1;
            break;
        }
        index += output_strings[i].size();
    }

    StringUtils::erase(outputs,index, count);

    outputs = StringUtils::insert(outputs,index, p_name);

    _apply_port_changes();
}

StringName VisualShaderNodeGroupBase::get_output_port_name(int p_id) const {
    ERR_FAIL_COND_V(!output_ports.contains(p_id), StringName())
    return StringName(output_ports.at(p_id).name);
}

int VisualShaderNodeGroupBase::get_free_input_port_id() const {
    return input_ports.size();
}

int VisualShaderNodeGroupBase::get_free_output_port_id() const {
    return output_ports.size();
}

void VisualShaderNodeGroupBase::set_control(Control *p_control, int p_index) {
    controls[p_index] = p_control;
}

Control *VisualShaderNodeGroupBase::get_control(int p_index) {
    ERR_FAIL_COND_V(!controls.contains(p_index), nullptr)
    return controls[p_index];
}

void VisualShaderNodeGroupBase::_apply_port_changes() {

    PODVector<se_string_view> inputs_strings = StringUtils::split(inputs,';', false);
    PODVector<se_string_view> outputs_strings = StringUtils::split(outputs,';', false);

    clear_input_ports();
    clear_output_ports();

    for (int i = 0; i < inputs_strings.size(); i++) {
        PODVector<se_string_view> arr = StringUtils::split(inputs_strings[i],',');
        ERR_FAIL_COND(arr.size() != 3)
        Port port;
        port.type = (PortType)StringUtils::to_int(arr[1]);
        port.name = arr[2];
        input_ports[i] = port;
    }
    for (int i = 0; i < outputs_strings.size(); i++) {
        PODVector<se_string_view> arr = StringUtils::split(outputs_strings[i],',');
        ERR_FAIL_COND(arr.size() != 3)
        Port port;
        port.type = (PortType)StringUtils::to_int(arr[1]);
        port.name = arr[2];
        output_ports[i] = port;
    }
}

void VisualShaderNodeGroupBase::set_editable(bool p_enabled) {
    editable = p_enabled;
}

bool VisualShaderNodeGroupBase::is_editable() const {
    return editable;
}

void VisualShaderNodeGroupBase::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_size", {"size"}), &VisualShaderNodeGroupBase::set_size);
    MethodBinder::bind_method(D_METHOD("get_size"), &VisualShaderNodeGroupBase::get_size);

    MethodBinder::bind_method(D_METHOD("set_inputs", {"inputs"}), &VisualShaderNodeGroupBase::set_inputs);
    MethodBinder::bind_method(D_METHOD("get_inputs"), &VisualShaderNodeGroupBase::get_inputs);

    MethodBinder::bind_method(D_METHOD("set_outputs", {"outputs"}), &VisualShaderNodeGroupBase::set_outputs);
    MethodBinder::bind_method(D_METHOD("get_outputs"), &VisualShaderNodeGroupBase::get_outputs);

    MethodBinder::bind_method(D_METHOD("is_valid_port_name", {"name"}), &VisualShaderNodeGroupBase::is_valid_port_name);

    MethodBinder::bind_method(D_METHOD("add_input_port", {"id", "type", "name"}), &VisualShaderNodeGroupBase::add_input_port);
    MethodBinder::bind_method(D_METHOD("remove_input_port", {"id"}), &VisualShaderNodeGroupBase::remove_input_port);
    MethodBinder::bind_method(D_METHOD("get_input_port_count"), &VisualShaderNodeGroupBase::get_input_port_count);
    MethodBinder::bind_method(D_METHOD("has_input_port", {"id"}), &VisualShaderNodeGroupBase::has_input_port);
    MethodBinder::bind_method(D_METHOD("clear_input_ports"), &VisualShaderNodeGroupBase::clear_input_ports);

    MethodBinder::bind_method(D_METHOD("add_output_port", {"id", "type", "name"}), &VisualShaderNodeGroupBase::add_output_port);
    MethodBinder::bind_method(D_METHOD("remove_output_port", {"id"}), &VisualShaderNodeGroupBase::remove_output_port);
    MethodBinder::bind_method(D_METHOD("get_output_port_count"), &VisualShaderNodeGroupBase::get_output_port_count);
    MethodBinder::bind_method(D_METHOD("has_output_port", {"id"}), &VisualShaderNodeGroupBase::has_output_port);
    MethodBinder::bind_method(D_METHOD("clear_output_ports"), &VisualShaderNodeGroupBase::clear_output_ports);

    MethodBinder::bind_method(D_METHOD("set_input_port_name", {"id", "name"}), &VisualShaderNodeGroupBase::set_input_port_name);
    MethodBinder::bind_method(D_METHOD("set_input_port_type", {"id", "type"}), &VisualShaderNodeGroupBase::set_input_port_type);
    MethodBinder::bind_method(D_METHOD("set_output_port_name", {"id", "name"}), &VisualShaderNodeGroupBase::set_output_port_name);
    MethodBinder::bind_method(D_METHOD("set_output_port_type", {"id", "type"}), &VisualShaderNodeGroupBase::set_output_port_type);

    MethodBinder::bind_method(D_METHOD("get_free_input_port_id"), &VisualShaderNodeGroupBase::get_free_input_port_id);
    MethodBinder::bind_method(D_METHOD("get_free_output_port_id"), &VisualShaderNodeGroupBase::get_free_output_port_id);

    MethodBinder::bind_method(D_METHOD("set_control", {"control", "index"}), &VisualShaderNodeGroupBase::set_control);
    MethodBinder::bind_method(D_METHOD("get_control", {"index"}), &VisualShaderNodeGroupBase::get_control);

    MethodBinder::bind_method(D_METHOD("set_editable", {"enabled"}), &VisualShaderNodeGroupBase::set_editable);
    MethodBinder::bind_method(D_METHOD("is_editable"), &VisualShaderNodeGroupBase::is_editable);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "editable"), "set_editable", "is_editable");
}

String VisualShaderNodeGroupBase::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
    return String();
}

VisualShaderNodeGroupBase::VisualShaderNodeGroupBase() {
    size = Size2(0, 0);
    inputs = "";
    outputs = "";
    editable = false;
}

////////////// Expression

se_string_view VisualShaderNodeExpression::get_caption() const {
    return String("Expression");
}

void VisualShaderNodeExpression::set_expression(const String &p_expression) {
    expression = p_expression;
}

void VisualShaderNodeExpression::build() {
    emit_changed();
}

String VisualShaderNodeExpression::get_expression() const {
    return expression;
}
static const char *pre_symbols[] = { "\t", ",", ";", "{", "[", "]", "(", " ", "-", "*", "/", "+", "=", "&", "|", "!" };
static const char *post_symbols[] = { "\t", "\n", ",", ";", "}", "[", "]", ")", " ", ".", "-", "*", "/", "+", "=", "&",
    "|", "!" };

String VisualShaderNodeExpression::generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {

    String _expression = expression;

    _expression = StringUtils::insert(_expression,0, String("\n"));
    _expression =StringUtils::replace(_expression,"\n", "\n\t\t");

    for (int i = 0; i < get_input_port_count(); i++) {
        for (const char *pre_sym : pre_symbols) {
            se_tmp_string<64, true> pre_sym_str(pre_sym);
            for (const char *post_sym : post_symbols) {
                _expression.replace(
                        pre_sym_str + get_input_port_name(i) + post_sym, pre_sym_str + p_input_vars[i] + post_sym);
            }
        }
    }
    for (int i = 0; i < get_output_port_count(); i++) {
        for (const char *pre_sym : pre_symbols) {
            se_tmp_string<64, true> pre_sym_str(pre_sym);
            for (const char *post_sym : post_symbols) {
                _expression.replace(
                        pre_sym_str + get_output_port_name(i) + post_sym, pre_sym_str + p_output_vars[i] + post_sym);
            }
        }
    }

    String output_initializer;

    for (int i = 0; i < get_output_port_count(); i++) {
        int port_type = get_output_port_type(i);
        String tk;
        switch (port_type) {
            case PORT_TYPE_SCALAR:
                tk = "0.0";
                break;
            case PORT_TYPE_VECTOR:
                tk = "vec3(0.0, 0.0, 0.0)";
                break;
            case PORT_TYPE_BOOLEAN:
                tk = "false";
                break;
            case PORT_TYPE_TRANSFORM:
                tk = "mat4(1.0)";
                break;
            default:
                continue;
        }
        output_initializer += "\t" + p_output_vars[i] + "=" + tk + ";\n";
    }

    String code;
    code += output_initializer;
    code += String("\t{");
    code += _expression;
    code += String("\n\t}");

    return code;
}

void VisualShaderNodeExpression::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_expression", {"expression"}), &VisualShaderNodeExpression::set_expression);
    MethodBinder::bind_method(D_METHOD("get_expression"), &VisualShaderNodeExpression::get_expression);

    MethodBinder::bind_method(D_METHOD("build"), &VisualShaderNodeExpression::build);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "expression"), "set_expression", "get_expression");
}

VisualShaderNodeExpression::VisualShaderNodeExpression() {
    expression = "";
    set_editable(true);
}

////////////// Global Expression

se_string_view VisualShaderNodeGlobalExpression::get_caption() const {
    return String("GlobalExpression");
}

String VisualShaderNodeGlobalExpression::generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const {
    return expression;
}

VisualShaderNodeGlobalExpression::VisualShaderNodeGlobalExpression() {
    set_editable(false);
}
