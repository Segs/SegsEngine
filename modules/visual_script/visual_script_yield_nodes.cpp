/*************************************************************************/
/*  visual_script_yield_nodes.cpp                                        */
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

#include "visual_script_yield_nodes.h"

#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/os/os.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "visual_script_nodes.h"

#include "EASTL/sort.h"

IMPL_GDCLASS(VisualScriptYield)
IMPL_GDCLASS(VisualScriptYieldSignal)

VARIANT_ENUM_CAST(VisualScriptYield::YieldMode)
VARIANT_ENUM_CAST(VisualScriptYieldSignal::CallMode);

//////////////////////////////////////////
////////////////YIELD///////////
//////////////////////////////////////////

int VisualScriptYield::get_output_sequence_port_count() const {

    return 1;
}

bool VisualScriptYield::has_input_sequence_port() const {

    return true;
}

int VisualScriptYield::get_input_value_port_count() const {

    return 0;
}
int VisualScriptYield::get_output_value_port_count() const {

    return 0;
}

se_string_view VisualScriptYield::get_output_sequence_port_text(int p_port) const {

    return se_string_view();
}

PropertyInfo VisualScriptYield::get_input_value_port_info(int p_idx) const {

    return PropertyInfo();
}

PropertyInfo VisualScriptYield::get_output_value_port_info(int p_idx) const {

    return PropertyInfo();
}

se_string_view VisualScriptYield::get_caption() const {

    return yield_mode == YIELD_RETURN ? "Yield" : "Wait";
}

se_string VisualScriptYield::get_text() const {

    switch (yield_mode) {
        case YIELD_RETURN: return null_se_string;
        case YIELD_FRAME: return ("Next Frame");
        case YIELD_PHYSICS_FRAME: return ("Next Physics Frame");
        case YIELD_WAIT: return rtos(wait_time) + " sec(s)";
    }

    return null_se_string;
}

class VisualScriptNodeInstanceYield : public VisualScriptNodeInstance {
public:
    VisualScriptYield::YieldMode mode;
    float wait_time;

    int get_working_memory_size() const override { return 1; } //yield needs at least 1
    //virtual bool is_output_port_unsequenced(int p_idx) const { return false; }
    //virtual bool get_output_port_unsequenced(int p_idx,Variant* r_value,Variant* p_working_mem,String &r_error) const { return false; }

    int step(const Variant **p_inputs, Variant **p_outputs, StartMode p_start_mode, Variant *p_working_mem, Variant::CallError &r_error, se_string &r_error_str) override {

        if (p_start_mode == START_MODE_RESUME_YIELD) {
            return 0; //resuming yield
        } else {
            //yield

            SceneTree *tree = object_cast<SceneTree>(OS::get_singleton()->get_main_loop());
            if (!tree) {
                r_error_str = "Main Loop is not SceneTree";
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
                return 0;
            }

            Ref<VisualScriptFunctionState> state(make_ref_counted<VisualScriptFunctionState>());

            int ret = STEP_YIELD_BIT;
            switch (mode) {

                case VisualScriptYield::YIELD_RETURN:
                    ret = STEP_EXIT_FUNCTION_BIT;
                    break; //return the yield
                case VisualScriptYield::YIELD_FRAME: state->connect_to_signal_sv(tree, "idle_frame", Array()); break;
                case VisualScriptYield::YIELD_PHYSICS_FRAME: state->connect_to_signal_sv(tree, "physics_frame", Array()); break;
                case VisualScriptYield::YIELD_WAIT: state->connect_to_signal_sv(tree->create_timer(wait_time).get(), "timeout", Array()); break;
            }

            *p_working_mem = state;

            return ret;
        }
    }
};

VisualScriptNodeInstance *VisualScriptYield::instance(VisualScriptInstance *p_instance) {

    VisualScriptNodeInstanceYield *instance = memnew(VisualScriptNodeInstanceYield);
    //instance->instance=p_instance;
    instance->mode = yield_mode;
    instance->wait_time = wait_time;
    return instance;
}

void VisualScriptYield::set_yield_mode(YieldMode p_mode) {

    if (yield_mode == p_mode)
        return;
    yield_mode = p_mode;
    ports_changed_notify();
    Object_change_notify(this);
}

VisualScriptYield::YieldMode VisualScriptYield::get_yield_mode() {

    return yield_mode;
}

void VisualScriptYield::set_wait_time(float p_time) {

    if (wait_time == p_time)
        return;
    wait_time = p_time;
    ports_changed_notify();
}

float VisualScriptYield::get_wait_time() {

    return wait_time;
}

void VisualScriptYield::_validate_property(PropertyInfo &property) const {

    if (property.name == "wait_time") {
        if (yield_mode != YIELD_WAIT) {
            property.usage = 0;
        }
    }
}

void VisualScriptYield::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_yield_mode", {"mode"}), &VisualScriptYield::set_yield_mode);
    MethodBinder::bind_method(D_METHOD("get_yield_mode"), &VisualScriptYield::get_yield_mode);

    MethodBinder::bind_method(D_METHOD("set_wait_time", {"sec"}), &VisualScriptYield::set_wait_time);
    MethodBinder::bind_method(D_METHOD("get_wait_time"), &VisualScriptYield::get_wait_time);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "mode", PROPERTY_HINT_ENUM, "Frame,Physics Frame,Time", PROPERTY_USAGE_NOEDITOR), "set_yield_mode", "get_yield_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "wait_time"), "set_wait_time", "get_wait_time");

    BIND_ENUM_CONSTANT(YIELD_FRAME)
    BIND_ENUM_CONSTANT(YIELD_PHYSICS_FRAME)
    BIND_ENUM_CONSTANT(YIELD_WAIT)
}

VisualScriptYield::VisualScriptYield() {

    yield_mode = YIELD_FRAME;
    wait_time = 1;
}

template <VisualScriptYield::YieldMode MODE>
static Ref<VisualScriptNode> create_yield_node(se_string_view p_name) {

    Ref<VisualScriptYield> node(make_ref_counted<VisualScriptYield>());
    node->set_yield_mode(MODE);
    return node;
}

///////////////////////////////////////////////////
////////////////YIELD SIGNAL//////////////////////
//////////////////////////////////////////////////

int VisualScriptYieldSignal::get_output_sequence_port_count() const {

    return 1;
}

bool VisualScriptYieldSignal::has_input_sequence_port() const {

    return true;
}

Node *VisualScriptYieldSignal::_get_base_node() const {

#ifdef TOOLS_ENABLED
    Ref<Script> script = get_visual_script();
    if (not script)
        return nullptr;

    MainLoop *main_loop = OS::get_singleton()->get_main_loop();
    SceneTree *scene_tree = object_cast<SceneTree>(main_loop);

    if (!scene_tree)
        return nullptr;

    Node *edited_scene = scene_tree->get_edited_scene_root();

    if (!edited_scene)
        return nullptr;

    Node *script_node = _find_script_node(edited_scene, edited_scene, script);

    if (!script_node)
        return nullptr;

    if (!script_node->has_node(base_path))
        return nullptr;

    Node *path_to = script_node->get_node(base_path);

    return path_to;
#else

    return NULL;
#endif
}

StringName VisualScriptYieldSignal::_get_base_type() const {

    if (call_mode == CALL_MODE_SELF && get_visual_script())
        return get_visual_script()->get_instance_base_type();
    else if (call_mode == CALL_MODE_NODE_PATH && get_visual_script()) {
        Node *path = _get_base_node();
        if (path)
            return path->get_class_name();
    }

    return base_type;
}

int VisualScriptYieldSignal::get_input_value_port_count() const {

    if (call_mode == CALL_MODE_INSTANCE)
        return 1;
    else
        return 0;
}
int VisualScriptYieldSignal::get_output_value_port_count() const {

    MethodInfo sr;

    if (!ClassDB::get_signal(_get_base_type(), signal, &sr))
        return 0;

    return sr.arguments.size();
}

se_string_view VisualScriptYieldSignal::get_output_sequence_port_text(int p_port) const {

    return se_string_view();
}

PropertyInfo VisualScriptYieldSignal::get_input_value_port_info(int p_idx) const {

    if (call_mode == CALL_MODE_INSTANCE)
        return PropertyInfo(VariantType::OBJECT, "instance");
    else
        return PropertyInfo();
}

PropertyInfo VisualScriptYieldSignal::get_output_value_port_info(int p_idx) const {

    MethodInfo sr;

    if (!ClassDB::get_signal(_get_base_type(), signal, &sr))
        return PropertyInfo(); //no signal
    ERR_FAIL_INDEX_V(p_idx, sr.arguments.size(), PropertyInfo())
    return sr.arguments[p_idx];
}

se_string_view VisualScriptYieldSignal::get_caption() const {

    static se_string_view cname[3] = {
        "WaitSignal",
        "WaitNodeSignal",
        "WaitInstanceSigna;",
    };

    return cname[call_mode];
}

se_string VisualScriptYieldSignal::get_text() const {

    if (call_mode == CALL_MODE_SELF)
        return "  " + se_string(signal) + "()";
    else
        return "  " + se_string(_get_base_type()) + "." + signal + "()";
}

void VisualScriptYieldSignal::set_base_type(const StringName &p_type) {

    if (base_type == p_type)
        return;

    base_type = p_type;

    Object_change_notify(this);
    ports_changed_notify();
}

StringName VisualScriptYieldSignal::get_base_type() const {

    return base_type;
}

void VisualScriptYieldSignal::set_signal(const StringName &p_type) {

    if (signal == p_type)
        return;

    signal = p_type;

    Object_change_notify(this);
    ports_changed_notify();
}
StringName VisualScriptYieldSignal::get_signal() const {

    return signal;
}

void VisualScriptYieldSignal::set_base_path(const NodePath &p_type) {

    if (base_path == p_type)
        return;

    base_path = p_type;

    Object_change_notify(this);
    ports_changed_notify();
}

NodePath VisualScriptYieldSignal::get_base_path() const {

    return base_path;
}

void VisualScriptYieldSignal::set_call_mode(CallMode p_mode) {

    if (call_mode == p_mode)
        return;

    call_mode = p_mode;

    Object_change_notify(this);
    ports_changed_notify();
}

VisualScriptYieldSignal::CallMode VisualScriptYieldSignal::get_call_mode() const {

    return call_mode;
}

void VisualScriptYieldSignal::_validate_property(PropertyInfo &property) const {

    if (property.name == "base_type") {
        if (call_mode != CALL_MODE_INSTANCE) {
            property.usage = PROPERTY_USAGE_NOEDITOR;
        }
    }

    if (property.name == "node_path") {
        if (call_mode != CALL_MODE_NODE_PATH) {
            property.usage = 0;
        } else {

            Node *bnode = _get_base_node();
            if (bnode) {
                property.hint_string = (se_string)bnode->get_path(); //convert to loong string
            }
        }
    }

    if (property.name == "signal") {
        property.hint = PROPERTY_HINT_ENUM;

        ListPOD<MethodInfo> methods;

        ClassDB::get_signal_list(_get_base_type(), &methods);

        PODVector<se_string_view> mstring;
        for(const MethodInfo & E : methods) {
            if (StringUtils::begins_with(E.name,"_"))
                continue;
            mstring.emplace_back(StringUtils::get_slice(E.name.asCString(),':', 0));
        }
        eastl::sort(mstring.begin(),mstring.end());

        property.hint_string = se_string::joined(mstring,",");
    }
}

void VisualScriptYieldSignal::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_base_type", {"base_type"}), &VisualScriptYieldSignal::set_base_type);
    MethodBinder::bind_method(D_METHOD("get_base_type"), &VisualScriptYieldSignal::get_base_type);

    MethodBinder::bind_method(D_METHOD("set_signal", {"signal"}), &VisualScriptYieldSignal::set_signal);
    MethodBinder::bind_method(D_METHOD("get_signal"), &VisualScriptYieldSignal::get_signal);

    MethodBinder::bind_method(D_METHOD("set_call_mode", {"mode"}), &VisualScriptYieldSignal::set_call_mode);
    MethodBinder::bind_method(D_METHOD("get_call_mode"), &VisualScriptYieldSignal::get_call_mode);

    MethodBinder::bind_method(D_METHOD("set_base_path", {"base_path"}), &VisualScriptYieldSignal::set_base_path);
    MethodBinder::bind_method(D_METHOD("get_base_path"), &VisualScriptYieldSignal::get_base_path);

//    String bt;
//    for (int i = 0; i < (int)VariantType::VARIANT_MAX; i++) {
//        if (i > 0)
//            bt += ",";

//        bt += Variant::get_type_name(VariantType(i));
//    }

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "call_mode", PROPERTY_HINT_ENUM, "Self,Node Path,Instance"), "set_call_mode", "get_call_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "base_type", PROPERTY_HINT_TYPE_STRING, "Object"), "set_base_type", "get_base_type");
    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "node_path", PROPERTY_HINT_NODE_PATH_TO_EDITED_NODE), "set_base_path", "get_base_path");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "signal"), "set_signal", "get_signal");

    BIND_ENUM_CONSTANT(CALL_MODE_SELF)
    BIND_ENUM_CONSTANT(CALL_MODE_NODE_PATH)
    BIND_ENUM_CONSTANT(CALL_MODE_INSTANCE)
}

class VisualScriptNodeInstanceYieldSignal : public VisualScriptNodeInstance {
public:
    VisualScriptYieldSignal::CallMode call_mode;
    NodePath node_path;
    int output_args;
    StringName signal;

    VisualScriptYieldSignal *node;
    VisualScriptInstance *instance;

    int get_working_memory_size() const override { return 1; }
    //virtual bool is_output_port_unsequenced(int p_idx) const { return false; }
    //virtual bool get_output_port_unsequenced(int p_idx,Variant* r_value,Variant* p_working_mem,String &r_error) const { return true; }

    int step(const Variant **p_inputs, Variant **p_outputs, StartMode p_start_mode, Variant *p_working_mem, Variant::CallError &r_error, se_string &r_error_str) override {

        if (p_start_mode == START_MODE_RESUME_YIELD) {
            return 0; //resuming yield
        } else {
            //yield

            Object *object = nullptr;

            switch (call_mode) {

                case VisualScriptYieldSignal::CALL_MODE_SELF: {

                    object = instance->get_owner_ptr();

                } break;
                case VisualScriptYieldSignal::CALL_MODE_NODE_PATH: {

                    Node *node = object_cast<Node>(instance->get_owner_ptr());
                    if (!node) {
                        r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
                        r_error_str = "Base object is not a Node!";
                        return 0;
                    }

                    Node *another = node->get_node(node_path);
                    if (!another) {
                        r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
                        r_error_str = "Path does not lead Node!";
                        return 0;
                    }

                    object = another;

                } break;
                case VisualScriptYieldSignal::CALL_MODE_INSTANCE: {

                    object = *p_inputs[0];
                    if (!object) {
                        r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
                        r_error_str = "Supplied instance input is null.";
                        return 0;
                    }

                } break;
            }

            Ref<VisualScriptFunctionState> state(make_ref_counted<VisualScriptFunctionState>());

            state->connect_to_signal(object, signal, Array());

            *p_working_mem = state;

            return STEP_YIELD_BIT;
        }
    }
};

VisualScriptNodeInstance *VisualScriptYieldSignal::instance(VisualScriptInstance *p_instance) {

    VisualScriptNodeInstanceYieldSignal *instance = memnew(VisualScriptNodeInstanceYieldSignal);
    instance->node = this;
    instance->instance = p_instance;
    instance->signal = signal;
    instance->call_mode = call_mode;
    instance->node_path = base_path;
    instance->output_args = get_output_value_port_count();
    return instance;
}
VisualScriptYieldSignal::VisualScriptYieldSignal() {

    call_mode = CALL_MODE_SELF;
    base_type = "Object";
}

template <VisualScriptYieldSignal::CallMode cmode>
static Ref<VisualScriptNode> create_yield_signal_node(const String &p_name) {

    Ref<VisualScriptYieldSignal> node(make_ref_counted<VisualScriptYieldSignal>());
    node->set_call_mode(cmode);
    return node;
}

void register_visual_script_yield_nodes() {

    VisualScriptLanguage::singleton->add_register_func("functions/wait/wait_frame", create_yield_node<VisualScriptYield::YIELD_FRAME>);
    VisualScriptLanguage::singleton->add_register_func("functions/wait/wait_physics_frame", create_yield_node<VisualScriptYield::YIELD_PHYSICS_FRAME>);
    VisualScriptLanguage::singleton->add_register_func("functions/wait/wait_time", create_yield_node<VisualScriptYield::YIELD_WAIT>);

    VisualScriptLanguage::singleton->add_register_func("functions/yield", create_yield_node<VisualScriptYield::YIELD_RETURN>);
    VisualScriptLanguage::singleton->add_register_func("functions/yield_signal", create_node_generic<VisualScriptYieldSignal>);
}
