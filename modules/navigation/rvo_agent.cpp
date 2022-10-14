/*************************************************************************/
/*  rvo_agent.cpp                                                        */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "rvo_agent.h"

#include "nav_map.h"

RvoAgent::RvoAgent() {
}

void RvoAgent::set_map(NavMap *p_map) {
    map = p_map;
}

bool RvoAgent::is_map_changed() {
    if (!map) {
        return false;
    }

    bool is_changed = map->get_map_update_id() != map_update_id;
    map_update_id = map->get_map_update_id();
    return is_changed;
}

void RvoAgent::set_callback(Callable &&cb) {
    callback = eastl::move(cb);
}

bool RvoAgent::has_callback() const {
    return callback.is_valid();
}

void RvoAgent::dispatch_callback() {
    if (callback.is_null()) {
        return;
    }
    Object *obj = callback.get_object();
    if (obj == nullptr) {
        callback = {};
        return;
    }

    Callable::CallError responseCallError;
    Variant ret;
    Variant new_velocity = Vector3(agent.newVelocity_.x(), agent.newVelocity_.y(), agent.newVelocity_.z());

    const Variant *vp[1] = { &new_velocity};
    callback.call(vp, 1,ret, responseCallError);
}
