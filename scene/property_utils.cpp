/*************************************************************************/
/*  property_utils.cpp                                                   */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "property_utils.h"

#include "core/class_db.h"
#include "core/core_string_names.h"
#include "core/engine.h"
#include "core/script_language.h"
#include "core/vector.h"
#include "editor/editor_node.h"
#include "scene/resources/packed_scene.h"

bool PropertyUtils::is_property_value_different(const Variant &p_a, const Variant &p_b) {
    if (p_a.get_type() == VariantType::FLOAT && p_b.get_type() == VariantType::FLOAT) {
        //this must be done because, as some scenes save as text, there might be a tiny difference in floats due to numerical error
        return !Math::is_equal_approx((float)p_a, (float)p_b);
    }

    // For our purposes, treating null object as NIL is the right thing to do
    const Variant &a = p_a.get_type() == VariantType::OBJECT && p_a.as<Object *>() == nullptr ? Variant() : p_a;
    const Variant &b = p_b.get_type() == VariantType::OBJECT && p_b.as<Object *>() == nullptr ? Variant() : p_b;
    return !a.deep_equal(b);
}

Variant PropertyUtils::get_property_default_value(const Object *p_object, const StringName &p_property,
        bool *r_is_valid, const Dequeue<SceneState::PackState> *p_states_stack_cache, bool p_update_exports,
        const Node *p_owner, bool *r_is_class_default) {
    // This function obeys the way property values are set when an object is instantiated,
    // which is the following (the latter wins):
    // 1. Default value from builtin class
    // 2. Default value from script exported variable (from the topmost script)
    // 3. Value overrides from the instantiation/inheritance stack

    if (r_is_class_default) {
        *r_is_class_default = false;
    }
    if (r_is_valid) {
        *r_is_valid = false;
    }

    Ref<Script> topmost_script;

    if (const Node *node = object_cast<Node>(p_object)) {
        // Check inheritance/instantiation ancestors
        const Dequeue<SceneState::PackState> &states_stack = p_states_stack_cache ? *p_states_stack_cache : get_node_states_stack(node, p_owner);
        for (int i = 0; i < states_stack.size(); ++i) {
            const SceneState::PackState &ia = states_stack[i];
            bool found = false;
            Variant value_in_ancestor = ia.state->get_property_value(ia.node, p_property, found);
            if (found) {
                if (r_is_valid) {
                    *r_is_valid = true;
                }
                return value_in_ancestor;
            }
            // Save script for later
            bool has_script = false;
            Variant script = ia.state->get_property_value(ia.node, CoreStringNames::get_singleton()->_script, has_script);
            if (has_script) {
                Ref<Script> scr = refFromVariant<Script>(script);
                if (scr) {
                    topmost_script = scr;
                }
            }
        }
    }

    // Let's see what default is set by the topmost script having a default, if any
    if (!topmost_script) {
        topmost_script = refFromRefPtr<Script>(p_object->get_script());
    }
    if (topmost_script) {
        Variant default_value;
        // Should be called in the editor only and not at runtime,
        // otherwise it can cause problems because of missing instance state support.
        if (p_update_exports && Engine::get_singleton()->is_editor_hint()) {
            topmost_script->update_exports();
        }
        if (topmost_script->get_property_default_value(p_property, default_value)) {
            if (r_is_valid) {
                *r_is_valid = true;
            }
            return default_value;
        }
    }

    // Fall back to the default from the native class
    if (r_is_class_default) {
        *r_is_class_default = true;
    }
    bool valid = false;
    Variant value = ClassDB::class_get_default_property_value(p_object->get_class_name(), p_property, &valid);
    if (valid) {
        if (r_is_valid) {
            *r_is_valid = true;
        }
        return value;
    }

    // Heuristically check if this is a synthetic property (whatever/0, whatever/1, etc.)
    // because they are not in the class DB yet must have a default (null).
    String prop_str = String(p_property);
    int p = prop_str.rfind("/");
    if (p != -1 && p < prop_str.length() - 1) {
        bool all_digits = true;
        for (int i = p + 1; i < prop_str.length(); i++) {
            if (prop_str[i] < '0' || prop_str[i] > '9') {
                all_digits = false;
                break;
            }
        }
        if (r_is_valid) {
            *r_is_valid = all_digits;
        }
    }
    return Variant();
}

// Like SceneState::PackState, but using a raw pointer to avoid the cost of
// updating the reference count during the internal work of the functions below
namespace {
struct _FastPackState {
    SceneState *state = nullptr;
    int node = -1;
};
} // namespace

static bool _collect_inheritance_chain(
        const Ref<SceneState> &p_state, const NodePath &p_path, FixedVector<_FastPackState, 12, true> &r_states_stack) {
    bool found = false;

    FixedVector<_FastPackState,12,true> inheritance_states;

    Ref<SceneState> state = p_state;
    while (state) {
        int node = state->find_node_by_path(p_path);
        if (node >= 0) {
            // This one has state for this node
            inheritance_states.push_back({ state.get(), node });
            found = true;
        }
        state = state->get_base_scene_state();
    }

    for (int i = inheritance_states.size() - 1; i >= 0; --i) {
        r_states_stack.push_back(inheritance_states[i]);
    }

    return found;
}

Dequeue<SceneState::PackState> PropertyUtils::get_node_states_stack(
        const Node *p_node, const Node *p_owner, bool *r_instanced_by_owner) {
    if (r_instanced_by_owner) {
        *r_instanced_by_owner = true;
    }

    FixedVector<_FastPackState, 12, true> states_stack;
    {
        const Node *owner = p_owner;
#ifdef TOOLS_ENABLED
        if (!p_owner && Engine::get_singleton()->is_editor_hint()) {
            owner = EditorNode::get_singleton()->get_edited_scene();
        }
#endif

        const Node *n = p_node;
        for (;n; n = n->get_owner()) {
            if (n == owner) {
                const Ref<SceneState> &state = n->get_scene_inherited_state();
                if (_collect_inheritance_chain(state, n->get_path_to(p_node), states_stack)) {
                    if (r_instanced_by_owner) {
                        *r_instanced_by_owner = false;
                    }
                }
                break;
            } else if (!n->get_filename().empty()) {
                const Ref<SceneState> &state = n->get_scene_instance_state();
                _collect_inheritance_chain(state, n->get_path_to(p_node), states_stack);
            }
        }
    }

    // Convert to the proper type for returning, inverting the vector on the go
    // (it was more convenient to fill the vector in reverse order)
    Dequeue<SceneState::PackState> states_stack_ret;
    {
        states_stack_ret.resize(states_stack.size());
        _FastPackState *ps = states_stack.data();
        for (int i = states_stack.size() - 1; i >= 0; --i) {
            states_stack_ret[i].state = eastl::move(Ref(ps->state));
            states_stack_ret[i].node = ps->node;
            ++ps;
        }
    }
    return states_stack_ret;
}
