/*************************************************************************/
/*  visual_shader.h                                                      */
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

#include "core/string_builder.h"
#include "core/se_string.h"
#include "scene/gui/control.h"
#include "scene/resources/shader.h"

class VisualShaderNodeUniform;
class VisualShaderNode;

class VisualShader : public Shader {
    GDCLASS(VisualShader,Shader)

public:
    enum Type {
        TYPE_VERTEX,
        TYPE_FRAGMENT,
        TYPE_LIGHT,
        TYPE_MAX
    };

    struct Connection {
        int from_node;
        int from_port;
        int to_node;
        int to_port;
    };

    struct DefaultTextureParam {
        StringName name;
        Ref<Resource> param; //TODO: SEGS: this was <Texture> but in VisualShaderNodeCubeMap::get_default_texture_parameters a CubeMap ( which is not a Texture ) was assigned to this
    };

private:
    union ConnectionKey {

        struct {
            uint64_t node : 32;
            uint64_t port : 32;
        };
        uint64_t key;
        bool operator<(const ConnectionKey &p_key) const {
            return key < p_key.key;
        }
    };
    struct Node {
        Ref<VisualShaderNode> node;
        Vector2 position;
    };

    struct Graph {
        Map<int, Node> nodes;
        List<Connection> connections;
    } graph[TYPE_MAX];

    mutable String previous_code;
    ShaderMode shader_mode;
    Vector2 graph_offset;

    struct RenderModeEnums {
        ShaderMode mode;
        const char *string;
    };

    DefHashMap<String, int> modes;
    Set<StringName> flags;
    volatile mutable bool dirty;

    static RenderModeEnums render_mode_enums[];


    Array _get_node_connections(Type p_type) const;
    void _queue_update();
    Error _write_node(Type p_type, StringBuilder &global_code, StringBuilder &global_code_per_node, Map<Type, StringBuilder> &global_code_per_func, StringBuilder &code, Vector<DefaultTextureParam> &def_tex_params, const VMap<ConnectionKey, const List<Connection>::Element *> &input_connections, const VMap<ConnectionKey, const List<Connection>::Element *> &output_connections, int node, Set<int> &processed, bool for_preview, Set<StringName> &r_classes) const;
    void _input_type_changed(Type p_type, int p_id);

protected:
    void _update_shader() const override;
    static void _bind_methods();

    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(ListPOD<PropertyInfo> *p_list) const;

public:
    enum {
        NODE_ID_INVALID = -1,
        NODE_ID_OUTPUT = 0,
    };

    void add_node(Type p_type, const Ref<VisualShaderNode> &p_node, const Vector2 &p_position, int p_id);
    void set_node_position(Type p_type, int p_id, const Vector2 &p_position);

    Vector2 get_node_position(Type p_type, int p_id) const;
    Ref<VisualShaderNode> get_node(Type p_type, int p_id) const;

    Vector<int> get_node_list(Type p_type) const;
    int get_valid_node_id(Type p_type) const;

    int find_node_id(Type p_type, const Ref<VisualShaderNode> &p_node) const;
    void remove_node(Type p_type, int p_id);

    bool is_node_connection(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) const;
    bool can_connect_nodes(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) const;
    Error connect_nodes(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port);
    void disconnect_nodes(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port);
    void connect_nodes_forced(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port);
    bool is_port_types_compatible(int p_a, int p_b) const;

    void rebuild();
    void get_node_connections(Type p_type, List<Connection> *r_connections) const;

    void set_mode(ShaderMode p_mode);
    ShaderMode get_mode() const override;

    bool is_text_shader() const override;

    void set_graph_offset(const Vector2 &p_offset);
    Vector2 get_graph_offset() const;

    String generate_preview_shader(Type p_type, int p_node, int p_port, Vector<DefaultTextureParam> &r_default_tex_params) const;

    String validate_port_name(se_string_view p_name, const List<StringName> &p_input_ports, const List<StringName> &p_output_ports) const;
    String validate_uniform_name(se_string_view p_name, const Ref<VisualShaderNodeUniform> &p_uniform) const;

    VisualShader();
};

///
///
///

class VisualShaderNode : public Resource {
    GDCLASS(VisualShaderNode,Resource)

    int port_preview;

    Map<int, Variant> default_input_values;

    Array _get_default_input_values() const;
    void _set_default_input_values(const Array &p_values);

protected:
    static void _bind_methods();

public:
    enum PortType {
        PORT_TYPE_SCALAR,
        PORT_TYPE_VECTOR,
        PORT_TYPE_BOOLEAN,
        PORT_TYPE_TRANSFORM,
        PORT_TYPE_SAMPLER,
        PORT_TYPE_MAX,
    };

    virtual se_string_view get_caption() const = 0;

    virtual int get_input_port_count() const = 0;
    virtual PortType get_input_port_type(int p_port) const = 0;
    virtual StringName get_input_port_name(int p_port) const = 0;

    void set_input_port_default_value(int p_port, const Variant &p_value);
    Variant get_input_port_default_value(int p_port) const; // if NIL (default if node does not set anything) is returned, it means no default value is wanted if disconnected, thus no input var must be supplied (empty string will be supplied)

    virtual int get_output_port_count() const = 0;
    virtual PortType get_output_port_type(int p_port) const = 0;
    virtual StringName get_output_port_name(int p_port) const = 0;

    virtual StringName get_input_port_default_hint(int p_port) const;

    void set_output_port_for_preview(int p_index);
    int get_output_port_for_preview() const;

    virtual bool is_port_separator(int p_index) const;

    virtual Vector<StringName> get_editable_properties() const;

    virtual Vector<VisualShader::DefaultTextureParam> get_default_texture_parameters(VisualShader::Type p_type, int p_id) const;
    virtual String generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const;
    virtual String generate_global_per_node(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const;
    virtual String generate_global_per_func(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const;
    virtual String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const = 0; //if no output is connected, the output var passed will be empty. if no input is connected and input is NIL, the input var passed will be empty

    virtual StringName get_warning(ShaderMode p_mode, VisualShader::Type p_type) const;

    VisualShaderNode();
};

class VisualShaderNodeCustom : public VisualShaderNode {
    GDCLASS(VisualShaderNodeCustom,VisualShaderNode)

    struct Port {
        StringName name;
        int type;
    };

    List<Port> input_ports;
    List<Port> output_ports;

    friend class VisualShaderEditor;

protected:
    se_string_view get_caption() const override;

    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

protected:
    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;
    String generate_global_per_node(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;

    static void _bind_methods();

public:
    VisualShaderNodeCustom();
    void update_ports();
};

/////

class VisualShaderNodeInput : public VisualShaderNode {
    GDCLASS(VisualShaderNodeInput,VisualShaderNode)

    friend class VisualShader;
    VisualShader::Type shader_type;
    ShaderMode shader_mode;

    struct Port {
        ShaderMode mode;
        VisualShader::Type shader_type;
        PortType type;
        const char *name;
        const char *string;
    };

    static const Port ports[];
    static const Port preview_ports[];

    StringName input_name;

protected:
    static void _bind_methods();
    void _validate_property(PropertyInfo &property) const override;

public:
    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    se_string_view get_caption() const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

    void set_input_name(StringName p_name);
    StringName get_input_name() const;
    String get_input_real_name() const;

    int get_input_index_count() const;
    PortType get_input_index_type(int p_index) const;
    StringName get_input_index_name(int p_index) const;

    PortType get_input_type_by_name(StringName p_name) const;

    Vector<StringName> get_editable_properties() const override;

    VisualShaderNodeInput();
};

///

class VisualShaderNodeOutput : public VisualShaderNode {
    GDCLASS(VisualShaderNodeOutput,VisualShaderNode)

public:
    friend class VisualShader;
    VisualShader::Type shader_type;
    ShaderMode shader_mode;

    struct Port {
        ShaderMode mode;
        VisualShader::Type shader_type;
        PortType type;
        const char *name;
        const char *string;
    };

    static const Port ports[];

public:
    int get_input_port_count() const override;
    PortType get_input_port_type(int p_port) const override;
    StringName get_input_port_name(int p_port) const override;
    Variant get_input_port_default_value(int p_port) const;

    int get_output_port_count() const override;
    PortType get_output_port_type(int p_port) const override;
    StringName get_output_port_name(int p_port) const override;

    bool is_port_separator(int p_index) const override;

    se_string_view get_caption() const override;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

    VisualShaderNodeOutput();
};

class VisualShaderNodeUniform : public VisualShaderNode {
    GDCLASS(VisualShaderNodeUniform,VisualShaderNode)
private:
    StringName uniform_name;

protected:
    static void _bind_methods();

public:
    void set_uniform_name(const StringName &p_name);
    StringName get_uniform_name() const;

    VisualShaderNodeUniform();
};

class VisualShaderNodeGroupBase : public VisualShaderNode {
    GDCLASS(VisualShaderNodeGroupBase,VisualShaderNode)

private:
    void _apply_port_changes();

protected:
    Vector2 size;
    String inputs;
    String outputs;
    bool editable;

    struct Port {
        PortType type;
        String name;
    };

    Map<int, Port> input_ports;
    Map<int, Port> output_ports;
    Map<int, Control *> controls;

protected:
    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    void set_size(const Vector2 &p_size);
    Vector2 get_size() const;

    void set_inputs(const String &p_inputs);
    String get_inputs() const;

    void set_outputs(const String &p_outputs);
    String get_outputs() const;

    bool is_valid_port_name(const String &p_name) const;

    void add_input_port(int p_id, int p_type, const String &p_name);
    void remove_input_port(int p_id);
    int get_input_port_count() const override;
    bool has_input_port(int p_id) const;
    void clear_input_ports();

    void add_output_port(int p_id, int p_type, const String &p_name);
    void remove_output_port(int p_id);
    int get_output_port_count() const override;
    bool has_output_port(int p_id) const;
    void clear_output_ports();

    void set_input_port_type(int p_id, int p_type);
    PortType get_input_port_type(int p_id) const override;
    void set_input_port_name(int p_id, const String &p_name);
    StringName get_input_port_name(int p_id) const override;

    void set_output_port_type(int p_id, int p_type);
    PortType get_output_port_type(int p_id) const override;
    void set_output_port_name(int p_id, const String &p_name);
    StringName get_output_port_name(int p_id) const override;

    int get_free_input_port_id() const;
    int get_free_output_port_id() const;

    void set_control(Control *p_control, int p_index);
    Control *get_control(int p_index);

    void set_editable(bool p_enabled);
    bool is_editable() const;

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

    VisualShaderNodeGroupBase();
};

class VisualShaderNodeExpression : public VisualShaderNodeGroupBase {
    GDCLASS(VisualShaderNodeExpression,VisualShaderNodeGroupBase)

protected:
    String expression;

    static void _bind_methods();

public:
    se_string_view get_caption() const override;

    void set_expression(const String &p_expression);
    String get_expression() const;

    void build();

    String generate_code(ShaderMode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

    VisualShaderNodeExpression();
};

class VisualShaderNodeGlobalExpression : public VisualShaderNodeExpression {
    GDCLASS(VisualShaderNodeGlobalExpression,VisualShaderNodeExpression)

public:
    se_string_view get_caption() const override;

    String generate_global(ShaderMode p_mode, VisualShader::Type p_type, int p_id) const override;

    VisualShaderNodeGlobalExpression();
};
