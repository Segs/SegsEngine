/*************************************************************************/
/*  object_client.cpp                                                    */
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

#include "object.h"

#include "object_private.h"
#include "class_db.h"
#include "script_language.h"



Error Object::connect(const StringName& p_signal, const Callable& p_callable, uint32_t p_flags) {
    ERR_FAIL_COND_V(p_callable.is_null(), ERR_INVALID_PARAMETER);

    Object* target_object = p_callable.get_object();
    ERR_FAIL_COND_V(!target_object, ERR_INVALID_PARAMETER);

    auto s = private_data->signal_map.find(p_signal);

    if (s==private_data->signal_map.end()) {
        bool signal_is_valid = ClassDB::has_signal(get_class_name(), p_signal);
        //check in script
        if (!signal_is_valid && !script.is_null()) {
            Ref script_ref(refFromRefPtr<Script>(script));
            if (script_ref->has_script_signal(p_signal)) {
                signal_is_valid = true;
            }
        }
        if (unlikely(!signal_is_valid)) {
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Signal connection failure",
                    DEBUG_STR("In Object of type '" + String(get_class()) + "': Attempt to connect nonexistent signal '" +
                              p_signal + "' to callable '" + (String)p_callable + "'."));
            return ERR_INVALID_PARAMETER;
        }
        s = private_data->signal_map.emplace(p_signal,SignalData()).first;
    }

    const Callable &target = p_callable;

    if (s->second.slot_map.contains(target)) {
        if (p_flags & ObjectNS::CONNECT_REFERENCE_COUNTED) {
            s->second.slot_map[target].reference_count++;
            return OK;
        }
        else {
            ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Signal '" + p_signal + "' is already connected to given callable '" + (String)p_callable + "' in that object.");
        }
    }

    SignalData::Slot slot;

    Connection conn;
    conn.callable = target;
    conn.signal = ::Signal(this, p_signal);
    conn.flags = p_flags;
    slot.conn = conn;
    auto &conns(target_object->private_data->connections);
    slot.cE = conns.emplace(conns.end(), eastl::move(conn));
    if (p_flags & ObjectNS::CONNECT_REFERENCE_COUNTED) {
        slot.reference_count = 1;
    }

    s->second.slot_map[target] = eastl::move(slot);

    return OK;
}
