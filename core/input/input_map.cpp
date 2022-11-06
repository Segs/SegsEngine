/*************************************************************************/
/*  input_map.cpp                                                        */
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

#include "input_map.h"

#include "core/dictionary.h"
#include "core/list.h"
#include "core/string_formatter.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/project_settings.h"
#include "core/method_bind.h"

namespace  {
Vector<Ref<InputEvent> >::iterator _find_event(InputMap::Action &p_action, const Ref<InputEvent> &p_event, bool p_exact_match = false, bool *p_pressed=nullptr, float *p_strength=nullptr, float *p_raw_strength = nullptr) {
    ERR_FAIL_COND_V(!p_event, p_action.inputs.end());

    for (auto iter = p_action.inputs.begin(); iter!=p_action.inputs.end(); ++iter) {

        const Ref<InputEvent> &e(*iter);

        //if (e.type != Ref<InputEvent>::KEY && e.device != p_event.device) -- unsure about the KEY comparison, why is this here?
        //    continue;

        int device = e->get_device();
        if (device == InputMap::ALL_DEVICES || device == p_event->get_device()) {
            if (p_exact_match && e->shortcut_match(p_event)) {
                return iter;
            } else if (!p_exact_match && e->action_match(p_event, p_exact_match, p_pressed, p_strength, p_raw_strength, p_action.deadzone)) {
                return iter;
            }
        }
    }

    return p_action.inputs.end();
}
}

IMPL_GDCLASS(InputMap)

InputMap *InputMap::singleton = nullptr;

int InputMap::ALL_DEVICES = -1;

void InputMap::_bind_methods() {

    SE_BIND_METHOD(InputMap,has_action);
    MethodBinder::bind_method(D_METHOD("get_actions"), &InputMap::_get_actions);
    MethodBinder::bind_method(D_METHOD("add_action", {"action", "deadzone"}), &InputMap::add_action, {DEFVAL(0.5f)});
    SE_BIND_METHOD(InputMap,erase_action);

    SE_BIND_METHOD(InputMap,action_set_deadzone);
    SE_BIND_METHOD(InputMap,action_get_deadzone);
    SE_BIND_METHOD(InputMap,action_add_event);
    SE_BIND_METHOD(InputMap,action_has_event);
    SE_BIND_METHOD(InputMap,action_erase_event);
    SE_BIND_METHOD(InputMap,action_erase_events);
    MethodBinder::bind_method(D_METHOD("get_action_list", {"action"}), &InputMap::_get_action_list);
    SE_BIND_METHOD(InputMap,event_is_action);
    SE_BIND_METHOD(InputMap,load_from_globals);
}

void InputMap::add_action(const StringName &p_action, float p_deadzone) {

    ERR_FAIL_COND_MSG(input_map.contains(p_action), "InputMap already has action '" + String(p_action) + "'.");
    input_map[p_action] = Action();
    static int last_id = 1;
    input_map[p_action].id = last_id;
    input_map[p_action].deadzone = p_deadzone;
    last_id++;
}

void InputMap::erase_action(const StringName &p_action) {
    ERR_FAIL_COND_MSG(!input_map.contains(p_action), suggest_actions(p_action));
    input_map.erase(p_action);
}

Array InputMap::_get_actions() {

    Array ret;
    Vector<StringName> actions(get_actions());
    if (actions.empty())
        return ret;

    for(const StringName &E : actions ) {
        ret.push_back(E);
    }

    return ret;
}

Vector<StringName> InputMap::get_actions() const {
    if (input_map.empty()) {
        return {};
    }
    return input_map.keys<wrap_allocator>();
}

bool InputMap::has_action(const StringName &p_action) const {

    return input_map.contains(p_action);
}

float InputMap::action_get_deadzone(const StringName &p_action) {
    ERR_FAIL_COND_V_MSG(!input_map.contains(p_action), 0.0f, suggest_actions(p_action));

    return input_map[p_action].deadzone;
}

void InputMap::action_set_deadzone(const StringName &p_action, float p_deadzone) {
    ERR_FAIL_COND_MSG(!input_map.contains(p_action), suggest_actions(p_action));

    input_map[p_action].deadzone = p_deadzone;
}

void InputMap::action_add_event(const StringName &p_action, const Ref<InputEvent> &p_event) {

    ERR_FAIL_COND_MSG(not p_event, "It's not a reference to a valid InputEvent object.");
    ERR_FAIL_COND_MSG(!input_map.contains(p_action), suggest_actions(p_action));

    auto &map_entry(input_map[p_action]);
    if (_find_event(map_entry, p_event, true)!=map_entry.inputs.end()) {
        return; // Already added.
    }

    map_entry.inputs.emplace_back(p_event);
}

bool InputMap::action_has_event(const StringName &p_action, const Ref<InputEvent> &p_event) {

    ERR_FAIL_COND_V_MSG(!input_map.contains(p_action), false, suggest_actions(p_action));
    return (_find_event(input_map[p_action], p_event, true) != input_map[p_action].inputs.end());
}

void InputMap::action_erase_event(const StringName &p_action, const Ref<InputEvent> &p_event) {

    ERR_FAIL_COND_MSG(!input_map.contains(p_action), suggest_actions(p_action));

    auto iter = _find_event(input_map[p_action], p_event, true);
    if (input_map[p_action].inputs.end()!=iter) {
        input_map[p_action].inputs.erase(iter);
        if (Input::get_singleton()->is_action_pressed(p_action)) {
            Input::get_singleton()->action_release(p_action);
        }
    }
}

void InputMap::action_erase_events(const StringName &p_action) {

    ERR_FAIL_COND_MSG(!input_map.contains(p_action), "Request for nonexistent InputMap action '" + String(p_action) + "'.");

    input_map[p_action].inputs.clear();
}

/**
 * Returns an nonexistent action error message with a suggestion of the closest
 * matching action name (if possible).
 */
String InputMap::suggest_actions(const StringName &p_action) const {

    Vector<StringName> actions = get_actions();
    StringName closest_action;
    float closest_similarity = 0.0;

    // Find the most action with the most similar name.
    for (const StringName & E : actions) {
        const float similarity = StringUtils::similarity(E,p_action);

        if (similarity > closest_similarity) {
            closest_action = E;
            closest_similarity = similarity;
        }
    }

    String error_message = FormatVE("The InputMap action \"%s\" doesn't exist.", p_action.asCString());

    if (closest_similarity >= 0.4) {
        // Only include a suggestion in the error message if it's similar enough.
        error_message += FormatVE(" Did you mean \"%s\"?", closest_action.asCString());
    }
    return error_message;
}

Array InputMap::_get_action_list(const StringName &p_action) {

    Array ret;
    const Vector<Ref<InputEvent> > *al = get_action_list(p_action);
    if (al) {
        for (const Ref<InputEvent> &E : *al) {
            ret.push_back(E);
        }
    }

    return ret;
}

const Vector<Ref<InputEvent> > *InputMap::get_action_list(const StringName &p_action) {

    const Map<StringName, Action>::iterator E = input_map.find(p_action);
    if (E==input_map.end())
        return nullptr;

    return &E->second.inputs;
}

bool InputMap::event_is_action(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match) const {
    return event_get_action_status(p_event, p_action, p_exact_match);
}

bool InputMap::event_get_action_status(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match, bool *p_pressed, float *p_strength, float *p_raw_strength) const {
    auto E = input_map.find(p_action);
    ERR_FAIL_COND_V_MSG(E==input_map.end(), false, suggest_actions(p_action));
    //TODO: SEGS: holding a ref to p_event is not needed here, a simpler object_cast(p_event.ptr) would work here
    Ref<InputEventAction> input_event_action(dynamic_ref_cast<InputEventAction>(p_event));
    if (input_event_action) {
        bool pressed = input_event_action->is_pressed();
        if (p_pressed != nullptr)
            *p_pressed = pressed;
        if (p_strength != nullptr)
            *p_strength = pressed ? input_event_action->get_strength() : 0.0f;
        return input_event_action->get_action() == p_action;
    }

    bool pressed;
    float strength;
    float raw_strength;
    Vector<Ref<InputEvent> >::iterator event = _find_event(E->second, p_event, p_exact_match,&pressed, &strength,&raw_strength);
    if (event != E->second.inputs.end()) {
        if (p_pressed != nullptr) {
            *p_pressed = pressed;
        }
        if (p_strength != nullptr) {
            *p_strength = strength;
        }
        if (p_raw_strength != nullptr) {
            *p_raw_strength = raw_strength;
        }
        return true;
    } else {
        return false;
    }
}



void InputMap::load_from_globals() {
    using namespace StringUtils;
    input_map.clear();

    Vector<PropertyInfo> pinfo;
    ProjectSettings::get_singleton()->get_property_list(&pinfo);

    for (const PropertyInfo &pi :pinfo) {

        if (!begins_with(pi.name,"input/")) {
            continue;
        }

        String name(pi.name.asCString());
        name = substr(name,find(name,"/") + 1, name.length());

        auto action {ProjectSettings::get_singleton()->getT<Dictionary>(pi.name)};
        float deadzone = action.has("deadzone") ? action["deadzone"].as<float>() : 0.5f;
        auto events { action["events"].as<Array>()};

        add_action(StringName(name), deadzone);
        for (int i = 0; i < events.size(); i++) {
            Ref<InputEvent> event = refFromVariant<InputEvent>(events[i]);
            if (!event) {
                continue;
            }
            action_add_event(StringName(name), event);
        }
    }
}
namespace {
void addActionKeys(InputMap &im,const StringName &n,std::initializer_list<KeyList> action_keys,bool shifted=false) {
    im.add_action(n);
    for(KeyList key : action_keys) {
        Ref<InputEventKey> k(make_ref_counted<InputEventKey>());
        k->set_keycode(key);
        if(shifted) {
            k->set_shift(true);
        }
        im.action_add_event(n, k);
    }

}
}
void InputMap::load_default() {

    addActionKeys(*this,StaticCString("ui_accept"),{KEY_ENTER,KEY_KP_ENTER,KEY_SPACE});
    addActionKeys(*this,StaticCString("ui_select"),{KEY_SPACE});
    addActionKeys(*this,StaticCString("ui_cancel"),{KEY_ESCAPE});
    addActionKeys(*this,StaticCString("ui_focus_next"),{KEY_TAB});
    addActionKeys(*this,StaticCString("ui_focus_prev"),{KEY_TAB},true);
    addActionKeys(*this,StaticCString("ui_left"),{KEY_LEFT});
    addActionKeys(*this,StaticCString("ui_right"),{KEY_RIGHT});
    addActionKeys(*this,StaticCString("ui_up"),{KEY_UP});
    addActionKeys(*this,StaticCString("ui_down"),{KEY_DOWN});

    addActionKeys(*this,StaticCString("ui_page_up"),{KEY_PAGEUP});
    addActionKeys(*this,StaticCString("ui_page_down"),{KEY_PAGEDOWN});
    addActionKeys(*this,StaticCString("ui_home"),{KEY_HOME});
    addActionKeys(*this,StaticCString("ui_end"),{KEY_END});
}

InputMap::InputMap() {

    ERR_FAIL_COND_MSG(singleton, "Singleton in InputMap already exist.");
    singleton = this;
}
