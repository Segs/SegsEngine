/*************************************************************************/
/*  visual_script_yield_nodes.h                                          */
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

#ifndef VISUAL_SCRIPT_YIELD_NODES_H
#define VISUAL_SCRIPT_YIELD_NODES_H

#include "visual_script.h"

class VisualScriptYield : public VisualScriptNode {

    GDCLASS(VisualScriptYield,VisualScriptNode)

public:
    enum YieldMode {
        YIELD_RETURN,
        YIELD_FRAME,
        YIELD_PHYSICS_FRAME,
        YIELD_WAIT

    };

private:
    YieldMode yield_mode;
    float wait_time;

protected:
    void _validate_property(PropertyInfo &property) const override;

    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    se_string_view get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    se_string_view get_caption() const override;
    se_string get_text() const override;
    const char *get_category() const override { return "functions"; }

    void set_yield_mode(YieldMode p_mode);
    YieldMode get_yield_mode();

    void set_wait_time(float p_time);
    float get_wait_time();

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptYield();
};

class VisualScriptYieldSignal : public VisualScriptNode {

    GDCLASS(VisualScriptYieldSignal,VisualScriptNode)

public:
    enum CallMode {
        CALL_MODE_SELF,
        CALL_MODE_NODE_PATH,
        CALL_MODE_INSTANCE,

    };

private:
    CallMode call_mode;
    StringName base_type;
    NodePath base_path;
    StringName signal;

    Node *_get_base_node() const;
    StringName _get_base_type() const;

protected:
    void _validate_property(PropertyInfo &property) const override;

    static void _bind_methods();

public:
    int get_output_sequence_port_count() const override;
    bool has_input_sequence_port() const override;

    se_string_view get_output_sequence_port_text(int p_port) const override;

    int get_input_value_port_count() const override;
    int get_output_value_port_count() const override;

    PropertyInfo get_input_value_port_info(int p_idx) const override;
    PropertyInfo get_output_value_port_info(int p_idx) const override;

    se_string_view get_caption() const override;
    se_string get_text() const override;
    const char *get_category() const override { return "functions"; }

    void set_base_type(const StringName &p_type);
    StringName get_base_type() const;

    void set_signal(const StringName &p_type);
    StringName get_signal() const;

    void set_base_path(const NodePath &p_type);
    NodePath get_base_path() const;

    void set_call_mode(CallMode p_mode);
    CallMode get_call_mode() const;

    VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance) override;

    VisualScriptYieldSignal();
};

void register_visual_script_yield_nodes();

#endif // VISUAL_SCRIPT_YIELD_NODES_H
