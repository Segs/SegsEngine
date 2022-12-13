/*************************************************************************/
/*  cull_instance.h                                                      */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "core/godot_export.h"
#include "core/engine_entities.h"

struct CullInstancePortalDirty {

};
struct CullInstancePortalModeDirty {};
struct GODOT_EXPORT CullInstanceComponent {

public:
    enum PortalMode : int8_t {
        PORTAL_MODE_STATIC, // not moving within a room
        PORTAL_MODE_DYNAMIC, //  moving within room
        PORTAL_MODE_ROAMING, // moving between rooms
        PORTAL_MODE_GLOBAL, // frustum culled only
        PORTAL_MODE_IGNORE, // don't show at all - e.g. manual bounds, hidden portals
    };
    void set_include_in_bound(bool p_enabled) { _include_in_bound = p_enabled; }
    bool get_include_in_bound() const { return _include_in_bound; }

    void set_allow_merging(bool p_enabled) { _allow_merging = p_enabled; }
    bool get_allow_merging() const { return _allow_merging; }

    void set_portal_autoplace_priority(int p_priority) { _portal_autoplace_priority = p_priority; }
    int get_portal_autoplace_priority() const { return _portal_autoplace_priority; }

    PortalMode _portal_mode = PORTAL_MODE_STATIC;
    bool _include_in_bound : 1;
    bool _allow_merging : 1;

    // Allows instances to prefer to be autoplaced
    // in specific RoomGroups. This allows building exteriors
    // to be autoplaced in outside RoomGroups, allowing a complete
    // exterior / interior of building in one reusable Scene.
    // The default value 0 gives no preference (chooses the highest priority).
    // All other values will autoplace in the selected RoomGroup priority by preference.
    int _portal_autoplace_priority = 0;

    CullInstanceComponent() : _include_in_bound(1),_allow_merging(1) {

    }
protected:
//  virtual void _refresh_portal_mode() = 0;

    static void _bind_methods();
};

struct ComponentOperations {
    void (*add_to_reflection)();
    void (*remove_from_reflection)();
};

const ComponentOperations *get_cull_instance_operations();


void set_portal_mode(CullInstanceComponent &self, CullInstanceComponent::PortalMode p_mode);
CullInstanceComponent::PortalMode get_portal_mode(const CullInstanceComponent &self);
