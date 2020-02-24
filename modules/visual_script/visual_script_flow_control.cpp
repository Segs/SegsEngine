/*************************************************************************/
/*  visual_script_flow_control.cpp                                       */
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

#include "visual_script_flow_control.h"

#include "core/io/resource_loader.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/os/keyboard.h"
#include "core/project_settings.h"
#include "core/translation_helpers.h"
#include "core/string_formatter.h"

IMPL_GDCLASS(VisualScriptReturn)
IMPL_GDCLASS(VisualScriptCondition)
IMPL_GDCLASS(VisualScriptWhile)
IMPL_GDCLASS(VisualScriptIterator)
IMPL_GDCLASS(VisualScriptSequence)
IMPL_GDCLASS(VisualScriptSwitch)
IMPL_GDCLASS(VisualScriptTypeCast)

//////////////////////////////////////////
////////////////RETURN////////////////////
//////////////////////////////////////////

int VisualScriptReturn::get_output_sequence_port_count() const {

    return 0;
}

bool VisualScriptReturn::has_input_sequence_port() const {

    return true;
}

int VisualScriptReturn::get_input_value_port_count() const {

    return with_value ? 1 : 0;
}
int VisualScriptReturn::get_output_value_port_count() const {

    return 0;
}

StringView VisualScriptReturn::get_output_sequence_port_text(int p_port) const {

    return nullptr;
}

PropertyInfo VisualScriptReturn::get_input_value_port_info(int p_idx) const {

    PropertyInfo pinfo;
    pinfo.name = "result";
    pinfo.type = type;
    return pinfo;
}
PropertyInfo VisualScriptReturn::get_output_value_port_info(int p_idx) const {
    return PropertyInfo();
}

StringView VisualScriptReturn::get_caption() const {

    return "Return";
}

String VisualScriptReturn::get_text() const {

    return get_name();
}

void VisualScriptReturn::set_return_type(VariantType p_type) {

    if (type == p_type)
        return;
    type = p_type;
    ports_changed_notify();
}

VariantType VisualScriptReturn::get_return_type() const {

    return type;
}

void VisualScriptReturn::set_enable_return_value(bool p_enable) {
    if (with_value == p_enable)
        return;

    with_value = p_enable;
    ports_changed_notify();
}

bool VisualScriptReturn::is_return_value_enabled() const {

    return with_value;
}

void VisualScriptReturn::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_return_type", {"type"}), &VisualScriptReturn::set_return_type);
    MethodBinder::bind_method(D_METHOD("get_return_type"), &VisualScriptReturn::get_return_type);
    MethodBinder::bind_method(D_METHOD("set_enable_return_value", {"enable"}), &VisualScriptReturn::set_enable_return_value);
    MethodBinder::bind_method(D_METHOD("is_return_value_enabled"), &VisualScriptReturn::is_return_value_enabled);
    char argt[7+(longest_variant_type_name+1)*(int)VariantType::VARIANT_MAX];
    fill_with_all_variant_types("Any",argt);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "return_enabled"), "set_enable_return_value", "is_return_value_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "return_type", PropertyHint::Enum, argt), "set_return_type", "get_return_type");
}

class VisualScriptNodeInstanceReturn : public VisualScriptNodeInstance {
public:
    VisualScriptReturn *node;
    VisualScriptInstance *instance;
    bool with_value;

    int get_working_memory_size() const override { return 1; }
    //virtual bool is_output_port_unsequenced(int p_idx) const { return false; }
    //virtual bool get_output_port_unsequenced(int p_idx,Variant* r_value,Variant* p_working_mem,String &r_error) const { return true; }

    int step(const Variant **p_inputs, Variant **p_outputs, StartMode p_start_mode, Variant *p_working_mem, Variant::CallError &r_error, String &r_error_str) override {

        if (with_value) {
            *p_working_mem = *p_inputs[0];
            return STEP_EXIT_FUNCTION_BIT;
        } else {
            *p_working_mem = Variant();
            return 0;
        }
    }
};

VisualScriptNodeInstance *VisualScriptReturn::instance(VisualScriptInstance *p_instance) {

    VisualScriptNodeInstanceReturn *instance = memnew(VisualScriptNodeInstanceReturn);
    instance->node = this;
    instance->instance = p_instance;
    instance->with_value = with_value;
    return instance;
}

VisualScriptReturn::VisualScriptReturn() {

    with_value = false;
    type = VariantType::NIL;
}

template <bool with_value>
static Ref<VisualScriptNode> create_return_node(StringView p_name) {

    Ref<VisualScriptReturn> node(make_ref_counted<VisualScriptReturn>());
    node->set_enable_return_value(with_value);
    return node;
}

//////////////////////////////////////////
////////////////CONDITION/////////////////
//////////////////////////////////////////

int VisualScriptCondition::get_output_sequence_port_count() const {

    return 3;
}

bool VisualScriptCondition::has_input_sequence_port() const {

    return true;
}

int VisualScriptCondition::get_input_value_port_count() const {

    return 1;
}
int VisualScriptCondition::get_output_value_port_count() const {

    return 0;
}

StringView VisualScriptCondition::get_output_sequence_port_text(int p_port) const {

    if (p_port == 0)
        return "true";
    else if (p_port == 1)
        return "false";
    else
        return "done";
}

PropertyInfo VisualScriptCondition::get_input_value_port_info(int p_idx) const {

    PropertyInfo pinfo;
    pinfo.name = "cond";
    pinfo.type = VariantType::BOOL;
    return pinfo;
}
PropertyInfo VisualScriptCondition::get_output_value_port_info(int p_idx) const {
    return PropertyInfo();
}

StringView VisualScriptCondition::get_caption() const {

    return "Condition";
}

String VisualScriptCondition::get_text() const {

    return "if (cond) is:  ";
}

void VisualScriptCondition::_bind_methods() {
}

class VisualScriptNodeInstanceCondition : public VisualScriptNodeInstance {
public:
    VisualScriptCondition *node;
    VisualScriptInstance *instance;

    //virtual int get_working_memory_size() const { return 1; }
    //virtual bool is_output_port_unsequenced(int p_idx) const { return false; }
    //virtual bool get_output_port_unsequenced(int p_idx,Variant* r_value,Variant* p_working_mem,String &r_error) const { return true; }

    int step(const Variant **p_inputs, Variant **p_outputs, StartMode p_start_mode, Variant *p_working_mem, Variant::CallError &r_error, String &r_error_str) override {

        if (p_start_mode == START_MODE_CONTINUE_SEQUENCE)
            return 2;
        else if (p_inputs[0]->operator bool())
            return 0 | STEP_FLAG_PUSH_STACK_BIT;
        else
            return 1 | STEP_FLAG_PUSH_STACK_BIT;
    }
};

VisualScriptNodeInstance *VisualScriptCondition::instance(VisualScriptInstance *p_instance) {

    VisualScriptNodeInstanceCondition *instance = memnew(VisualScriptNodeInstanceCondition);
    instance->node = this;
    instance->instance = p_instance;
    return instance;
}

VisualScriptCondition::VisualScriptCondition() {
}

//////////////////////////////////////////
////////////////WHILE/////////////////
//////////////////////////////////////////

int VisualScriptWhile::get_output_sequence_port_count() const {

    return 2;
}

bool VisualScriptWhile::has_input_sequence_port() const {

    return true;
}

int VisualScriptWhile::get_input_value_port_count() const {

    return 1;
}
int VisualScriptWhile::get_output_value_port_count() const {

    return 0;
}

StringView VisualScriptWhile::get_output_sequence_port_text(int p_port) const {

    if (p_port == 0)
        return "repeat";
    else
        return "exit";
}

PropertyInfo VisualScriptWhile::get_input_value_port_info(int p_idx) const {

    PropertyInfo pinfo;
    pinfo.name = "cond";
    pinfo.type = VariantType::BOOL;
    return pinfo;
}
PropertyInfo VisualScriptWhile::get_output_value_port_info(int p_idx) const {
    return PropertyInfo();
}

StringView VisualScriptWhile::get_caption() const {

    return "While";
}

String VisualScriptWhile::get_text() const {

    return "while (cond): ";
}

void VisualScriptWhile::_bind_methods() {
}

class VisualScriptNodeInstanceWhile : public VisualScriptNodeInstance {
public:
    VisualScriptWhile *node;
    VisualScriptInstance *instance;

    //virtual int get_working_memory_size() const { return 1; }
    //virtual bool is_output_port_unsequenced(int p_idx) const { return false; }
    //virtual bool get_output_port_unsequenced(int p_idx,Variant* r_value,Variant* p_working_mem,String &r_error) const { return true; }

    int step(const Variant **p_inputs, Variant **p_outputs, StartMode p_start_mode, Variant *p_working_mem, Variant::CallError &r_error, String &r_error_str) override {

        bool keep_going = p_inputs[0]->operator bool();

        if (keep_going)
            return 0 | STEP_FLAG_PUSH_STACK_BIT;
        else
            return 1;
    }
};

VisualScriptNodeInstance *VisualScriptWhile::instance(VisualScriptInstance *p_instance) {

    VisualScriptNodeInstanceWhile *instance = memnew(VisualScriptNodeInstanceWhile);
    instance->node = this;
    instance->instance = p_instance;
    return instance;
}
VisualScriptWhile::VisualScriptWhile() {
}

//////////////////////////////////////////
////////////////ITERATOR/////////////////
//////////////////////////////////////////

int VisualScriptIterator::get_output_sequence_port_count() const {

    return 2;
}

bool VisualScriptIterator::has_input_sequence_port() const {

    return true;
}

int VisualScriptIterator::get_input_value_port_count() const {

    return 1;
}
int VisualScriptIterator::get_output_value_port_count() const {

    return 1;
}

StringView VisualScriptIterator::get_output_sequence_port_text(int p_port) const {

    if (p_port == 0)
        return "each";
    else
        return "exit";
}

PropertyInfo VisualScriptIterator::get_input_value_port_info(int p_idx) const {

    PropertyInfo pinfo;
    pinfo.name = "input";
    pinfo.type = VariantType::NIL;
    return pinfo;
}
PropertyInfo VisualScriptIterator::get_output_value_port_info(int p_idx) const {
    PropertyInfo pinfo;
    pinfo.name = "elem";
    pinfo.type = VariantType::NIL;
    return pinfo;
}
StringView VisualScriptIterator::get_caption() const {

    return "Iterator";
}

String VisualScriptIterator::get_text() const {

    return "for (elem) in (input): ";
}

void VisualScriptIterator::_bind_methods() {
}

class VisualScriptNodeInstanceIterator : public VisualScriptNodeInstance {
public:
    VisualScriptIterator *node;
    VisualScriptInstance *instance;

    int get_working_memory_size() const override { return 2; }
    //virtual bool is_output_port_unsequenced(int p_idx) const { return false; }
    //virtual bool get_output_port_unsequenced(int p_idx,Variant* r_value,Variant* p_working_mem,String &r_error) const { return true; }

    int step(const Variant **p_inputs, Variant **p_outputs, StartMode p_start_mode, Variant *p_working_mem, Variant::CallError &r_error, String &r_error_str) override {

        if (p_start_mode == START_MODE_BEGIN_SEQUENCE) {
            p_working_mem[0] = *p_inputs[0];
            bool valid;
            bool can_iter = p_inputs[0]->iter_init(p_working_mem[1], valid);

            if (!valid) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
                r_error_str = RTR_utf8("Input type not iterable: ") + Variant::get_type_name(p_inputs[0]->get_type());
                return 0;
            }

            if (!can_iter)
                return 1; //nothing to iterate

            *p_outputs[0] = p_working_mem[0].iter_get(p_working_mem[1], valid);

            if (!valid) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
                r_error_str = RTR_utf8("Iterator became invalid");
                return 0;
            }

        } else { //continue sequence

            bool valid;
            bool can_iter = p_working_mem[0].iter_next(p_working_mem[1], valid);

            if (!valid) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
                r_error_str = RTR_utf8("Iterator became invalid: ") + Variant::get_type_name(p_inputs[0]->get_type());
                return 0;
            }

            if (!can_iter)
                return 1; //nothing to iterate

            *p_outputs[0] = p_working_mem[0].iter_get(p_working_mem[1], valid);

            if (!valid) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
                r_error_str = RTR_utf8("Iterator became invalid");
                return 0;
            }
        }

        return 0 | STEP_FLAG_PUSH_STACK_BIT; //go around
    }
};

VisualScriptNodeInstance *VisualScriptIterator::instance(VisualScriptInstance *p_instance) {

    VisualScriptNodeInstanceIterator *instance = memnew(VisualScriptNodeInstanceIterator);
    instance->node = this;
    instance->instance = p_instance;
    return instance;
}

VisualScriptIterator::VisualScriptIterator() {
}

//////////////////////////////////////////
////////////////SEQUENCE/////////////////
//////////////////////////////////////////

int VisualScriptSequence::get_output_sequence_port_count() const {

    return steps;
}

bool VisualScriptSequence::has_input_sequence_port() const {

    return true;
}

int VisualScriptSequence::get_input_value_port_count() const {

    return 0;
}
int VisualScriptSequence::get_output_value_port_count() const {

    return 1;
}

StringView VisualScriptSequence::get_output_sequence_port_text(int p_port) const {
    static String v(::to_string(p_port+1));
    return v;
}

PropertyInfo VisualScriptSequence::get_input_value_port_info(int p_idx) const {
    return PropertyInfo();
}
PropertyInfo VisualScriptSequence::get_output_value_port_info(int p_idx) const {
    return PropertyInfo(VariantType::INT, "current");
}
StringView VisualScriptSequence::get_caption() const {

    return ("Sequence");
}

String VisualScriptSequence::get_text() const {

    return ("in order: ");
}

void VisualScriptSequence::set_steps(int p_steps) {

    ERR_FAIL_COND(p_steps < 1);
    if (steps == p_steps)
        return;

    steps = p_steps;
    ports_changed_notify();
}

int VisualScriptSequence::get_steps() const {

    return steps;
}

void VisualScriptSequence::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_steps", {"steps"}), &VisualScriptSequence::set_steps);
    MethodBinder::bind_method(D_METHOD("get_steps"), &VisualScriptSequence::get_steps);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "steps", PropertyHint::Range, "1,64,1"), "set_steps", "get_steps");
}

class VisualScriptNodeInstanceSequence : public VisualScriptNodeInstance {
public:
    VisualScriptSequence *node;
    VisualScriptInstance *instance;
    int steps;

    int get_working_memory_size() const override { return 1; }
    //virtual bool is_output_port_unsequenced(int p_idx) const { return false; }
    //virtual bool get_output_port_unsequenced(int p_idx,Variant* r_value,Variant* p_working_mem,String &r_error) const { return true; }

    int step(const Variant **p_inputs, Variant **p_outputs, StartMode p_start_mode, Variant *p_working_mem, Variant::CallError &r_error, String &r_error_str) override {

        if (p_start_mode == START_MODE_BEGIN_SEQUENCE) {

            p_working_mem[0] = 0;
        }

        int step = p_working_mem[0];

        *p_outputs[0] = step;

        if (step + 1 == steps)
            return step;
        else {
            p_working_mem[0] = step + 1;
            return step | STEP_FLAG_PUSH_STACK_BIT;
        }
    }
};

VisualScriptNodeInstance *VisualScriptSequence::instance(VisualScriptInstance *p_instance) {

    VisualScriptNodeInstanceSequence *instance = memnew(VisualScriptNodeInstanceSequence);
    instance->node = this;
    instance->instance = p_instance;
    instance->steps = steps;
    return instance;
}
VisualScriptSequence::VisualScriptSequence() {

    steps = 1;
}

//////////////////////////////////////////
////////////////EVENT TYPE FILTER///////////
//////////////////////////////////////////

int VisualScriptSwitch::get_output_sequence_port_count() const {

    return case_values.size() + 1;
}

bool VisualScriptSwitch::has_input_sequence_port() const {

    return true;
}

int VisualScriptSwitch::get_input_value_port_count() const {

    return case_values.size() + 1;
}
int VisualScriptSwitch::get_output_value_port_count() const {

    return 0;
}

StringView VisualScriptSwitch::get_output_sequence_port_text(int p_port) const {

    if (p_port == case_values.size())
        return "done";

    return nullptr;
}

PropertyInfo VisualScriptSwitch::get_input_value_port_info(int p_idx) const {

    if (p_idx < case_values.size()) {
        return PropertyInfo(case_values[p_idx].type, " =");
    } else
        return PropertyInfo(VariantType::NIL, "input");
}

PropertyInfo VisualScriptSwitch::get_output_value_port_info(int p_idx) const {

    return PropertyInfo();
}

StringView VisualScriptSwitch::get_caption() const {

    return ("Switch");
}

String VisualScriptSwitch::get_text() const {

    return ("'input' is:");
}

class VisualScriptNodeInstanceSwitch : public VisualScriptNodeInstance {
public:
    VisualScriptInstance *instance;
    int case_count;

    //virtual int get_working_memory_size() const { return 0; }
    //virtual bool is_output_port_unsequenced(int p_idx) const { return false; }
    //virtual bool get_output_port_unsequenced(int p_idx,Variant* r_value,Variant* p_working_mem,String &r_error) const { return false; }

    int step(const Variant **p_inputs, Variant **p_outputs, StartMode p_start_mode, Variant *p_working_mem, Variant::CallError &r_error, String &r_error_str) override {

        if (p_start_mode == START_MODE_CONTINUE_SEQUENCE) {
            return case_count; //exit
        }

        for (int i = 0; i < case_count; i++) {

            if (*p_inputs[i] == *p_inputs[case_count]) {
                return i | STEP_FLAG_PUSH_STACK_BIT;
            }
        }

        return case_count;
    }
};

VisualScriptNodeInstance *VisualScriptSwitch::instance(VisualScriptInstance *p_instance) {

    VisualScriptNodeInstanceSwitch *instance = memnew(VisualScriptNodeInstanceSwitch);
    instance->instance = p_instance;
    instance->case_count = case_values.size();
    return instance;
}

bool VisualScriptSwitch::_set(const StringName &p_name, const Variant &p_value) {

    if (p_name == "case_count") {
        case_values.resize(p_value);
        Object_change_notify(this);
        ports_changed_notify();
        return true;
    }

    if (StringUtils::begins_with(p_name,"case/")) {

        int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
        ERR_FAIL_INDEX_V(idx, case_values.size(), false);

        case_values[idx].type = VariantType(int(p_value));
        Object_change_notify(this);
        ports_changed_notify();

        return true;
    }

    return false;
}

bool VisualScriptSwitch::_get(const StringName &p_name, Variant &r_ret) const {

    if (p_name == "case_count") {
        r_ret = case_values.size();
        return true;
    }

    if (StringUtils::begins_with(p_name,"case/")) {

        int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
        ERR_FAIL_INDEX_V(idx, case_values.size(), false);

        r_ret = case_values[idx].type;
        return true;
    }

    return false;
}
void VisualScriptSwitch::_get_property_list(Vector<PropertyInfo> *p_list) const {

    p_list->push_back(PropertyInfo(VariantType::INT, "case_count", PropertyHint::Range, "0,128"));

    char argt[7+(longest_variant_type_name+1)*(int)VariantType::VARIANT_MAX];
    fill_with_all_variant_types("Any",argt);

    for (int i = 0; i < case_values.size(); i++) {
        p_list->push_back(PropertyInfo(VariantType::INT, StringName("case/" + itos(i)), PropertyHint::Enum, argt));
    }
}

void VisualScriptSwitch::_bind_methods() {
}

VisualScriptSwitch::VisualScriptSwitch() {
}

//////////////////////////////////////////
////////////////TYPE CAST///////////
//////////////////////////////////////////

int VisualScriptTypeCast::get_output_sequence_port_count() const {

    return 2;
}

bool VisualScriptTypeCast::has_input_sequence_port() const {

    return true;
}

int VisualScriptTypeCast::get_input_value_port_count() const {

    return 1;
}
int VisualScriptTypeCast::get_output_value_port_count() const {

    return 1;
}

StringView VisualScriptTypeCast::get_output_sequence_port_text(int p_port) const {

    return p_port == 0 ? "yes" : "no";
}

PropertyInfo VisualScriptTypeCast::get_input_value_port_info(int p_idx) const {

    return PropertyInfo(VariantType::OBJECT, "instance");
}

PropertyInfo VisualScriptTypeCast::get_output_value_port_info(int p_idx) const {

    return PropertyInfo(VariantType::OBJECT, "", PropertyHint::TypeString, get_base_type());
}

StringView VisualScriptTypeCast::get_caption() const {

    return "Type Cast";
}

String VisualScriptTypeCast::get_text() const {

    StringView sc(base_type);
    if (!script.empty())
        sc = PathUtils::get_file(script);
    return FormatVE("Is %.*s?",sc.size(),sc.data());
}

void VisualScriptTypeCast::set_base_type(const StringName &p_type) {

    if (base_type == p_type)
        return;

    base_type = p_type;
    Object_change_notify(this);
    ports_changed_notify();
}

StringName VisualScriptTypeCast::get_base_type() const {

    return base_type;
}

void VisualScriptTypeCast::set_base_script(StringView p_path) {

    if (script == p_path)
        return;

    script = p_path;
    Object_change_notify(this);
    ports_changed_notify();
}
const String &VisualScriptTypeCast::get_base_script() const {

    return script;
}

VisualScriptTypeCast::TypeGuess VisualScriptTypeCast::guess_output_type(TypeGuess *p_inputs, int p_output) const {

    TypeGuess tg;
    tg.type = VariantType::OBJECT;
    if (!script.empty()) {
        tg.script = dynamic_ref_cast<Script>(ResourceLoader::load(script));
    }
    //if (not tg.script) {
    //	tg.gdclass = base_type;
    //}

    return tg;
}

class VisualScriptNodeInstanceTypeCast : public VisualScriptNodeInstance {
public:
    VisualScriptInstance *instance;
    StringName base_type;
    String script;

    //virtual int get_working_memory_size() const { return 0; }
    //virtual bool is_output_port_unsequenced(int p_idx) const { return false; }
    //virtual bool get_output_port_unsequenced(int p_idx,Variant* r_value,Variant* p_working_mem,String &r_error) const { return false; }

    int step(const Variant **p_inputs, Variant **p_outputs, StartMode p_start_mode, Variant *p_working_mem, Variant::CallError &r_error, String &r_error_str) override {

        Object *obj = *p_inputs[0];

        *p_outputs[0] = Variant();

        if (!obj) {
            r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
            r_error_str = "Instance is null";
            return 0;
        }

        if (!script.empty()) {

            Ref<Script> obj_script = refFromRefPtr<Script>(obj->get_script());
            if (!obj_script) {
                return 1; //well, definitely not the script because object we got has no script.
            }

            if (!ResourceCache::has(script)) {
                //if the script is not in use by anyone, we can safely assume whathever we got is not casting to it.
                return 1;
            }
            Ref<Script> cast_script(dynamic_ref_cast<Script>(RES(ResourceCache::get(script))));
            if (!cast_script) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
                r_error_str = "Script path is not a script: " + script;
                return 1;
            }

            while (obj_script) {

                if (cast_script == obj_script) {
                    *p_outputs[0] = *p_inputs[0]; //copy
                    return 0; // it is the script, yey
                }

                obj_script = obj_script->get_base_script();
            }

            return 1; //not found sorry
        }

        if (ClassDB::is_parent_class(obj->get_class_name(), base_type)) {
            *p_outputs[0] = *p_inputs[0]; //copy
            return 0;
        } else
            return 1;
    }
};

VisualScriptNodeInstance *VisualScriptTypeCast::instance(VisualScriptInstance *p_instance) {

    VisualScriptNodeInstanceTypeCast *instance = memnew(VisualScriptNodeInstanceTypeCast);
    instance->instance = p_instance;
    instance->base_type = base_type;
    instance->script = script;
    return instance;
}

void VisualScriptTypeCast::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_base_type", {"type"}), &VisualScriptTypeCast::set_base_type);
    MethodBinder::bind_method(D_METHOD("get_base_type"), &VisualScriptTypeCast::get_base_type);

    MethodBinder::bind_method(D_METHOD("set_base_script", {"path"}), &VisualScriptTypeCast::set_base_script);
    MethodBinder::bind_method(D_METHOD("get_base_script"), &VisualScriptTypeCast::get_base_script);

    Vector<String> script_extensions;
    for (int i = 0; i < ScriptServer::get_language_count(); i++) {
        ScriptServer::get_language(i)->get_recognized_extensions(&script_extensions);
    }

    String script_ext_hint;
    for (const String &E : script_extensions) {
        if (!script_ext_hint.empty())
            script_ext_hint += ",";
        script_ext_hint += "*." + E;
    }

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "base_type", PropertyHint::TypeString, "Object"), "set_base_type", "get_base_type");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "base_script", PropertyHint::File, script_ext_hint), "set_base_script", "get_base_script");
}

VisualScriptTypeCast::VisualScriptTypeCast() {

    base_type = "Object";
}

void register_visual_script_flow_control_nodes() {

    VisualScriptLanguage::singleton->add_register_func("flow_control/return", create_return_node<false>);
    VisualScriptLanguage::singleton->add_register_func("flow_control/return_with_value", create_return_node<true>);
    VisualScriptLanguage::singleton->add_register_func("flow_control/condition", create_node_generic<VisualScriptCondition>);
    VisualScriptLanguage::singleton->add_register_func("flow_control/while", create_node_generic<VisualScriptWhile>);
    VisualScriptLanguage::singleton->add_register_func("flow_control/iterator", create_node_generic<VisualScriptIterator>);
    VisualScriptLanguage::singleton->add_register_func("flow_control/sequence", create_node_generic<VisualScriptSequence>);
    VisualScriptLanguage::singleton->add_register_func("flow_control/switch", create_node_generic<VisualScriptSwitch>);
    //VisualScriptLanguage::singleton->add_register_func("flow_control/input_filter", create_node_generic<VisualScriptInputFilter>);
    VisualScriptLanguage::singleton->add_register_func("flow_control/type_cast", create_node_generic<VisualScriptTypeCast>);
}
