/*************************************************************************/
/*  cull_instance.cpp                                                    */
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

#include "cull_instance_component.h"

#include "core/class_db.h"
#include "core/ecs_registry.h"
#include "core/method_enum_caster.h"
#include <entt/core/hashed_string.hpp>
#include <entt/meta/factory.hpp>
#include <entt/meta/resolve.hpp>

VARIANT_ENUM_CAST(CullInstanceComponent::PortalMode);


CullInstanceComponent::PortalMode get_portal_mode(const CullInstanceComponent &self) {
    return self._portal_mode;
}
void set_portal_mode(CullInstanceComponent &self, CullInstanceComponent::PortalMode p_mode) {
    self._portal_mode = p_mode;
    assert(false);
    //_refresh_portal_mode();
}
static void _bind_methods() {
    using namespace entt;
    {
        ENTT_START_REFL(CullInstanceComponent::PortalMode);
        ENTT_ENUM_REFL(CullInstanceComponent::PortalMode, PORTAL_MODE_STATIC, "Static");
        ENTT_ENUM_REFL(CullInstanceComponent::PortalMode, PORTAL_MODE_DYNAMIC, "Dynamic");
        ENTT_ENUM_REFL(CullInstanceComponent::PortalMode, PORTAL_MODE_ROAMING, "Roaming");
        ENTT_ENUM_REFL(CullInstanceComponent::PortalMode, PORTAL_MODE_GLOBAL, "Global");
        ENTT_ENUM_REFL(CullInstanceComponent::PortalMode, PORTAL_MODE_IGNORE, "Ignore");
        ENTT_END_REFL();
    }

    {
        static const PropertyGroupInfo property_groups[] = { { "Portals", "" } };
        ENTT_START_REFL(CullInstanceComponent).prop(g_property_groups, Span<const PropertyGroupInfo>(property_groups));
        ENTT_FUNCTION_REFL(set_portal_mode);
        ENTT_FUNCTION_REFL(get_portal_mode);
        ENTT_METHOD_REFL(set_include_in_bound);
        ENTT_METHOD_REFL(get_include_in_bound);
        ENTT_METHOD_REFL(set_portal_autoplace_priority);
        ENTT_METHOD_REFL(get_portal_autoplace_priority);
        ENTT_METHOD_REFL(set_allow_merging);
        ENTT_METHOD_REFL(get_allow_merging);

        auto group_prop = ENTT_GROUP_PROPERTY_IDX(0);

        //ENTT_PROPERTY_EX_REFL(set_portal_mode, get_portal_mode, "portal_mode", group_prop, ENTT_DEFAULT_PROPERTY_VALUE(CullInstanceComponent::PortalMode::PORTAL_MODE_STATIC));
        ENTT_PROPERTY_EX_REFL(ENTT_FUNCTION_ACCESSORS(set_portal_mode, get_portal_mode), "portal_mode", group_prop,
                ENTT_DEFAULT_PROPERTY_VALUE(CullInstanceComponent::PortalMode::PORTAL_MODE_STATIC));
        ENTT_PROPERTY_EX_REFL(ENTT_MEMBER_ACCESSORS(set_include_in_bound, get_include_in_bound), "include_in_bound", group_prop, ENTT_DEFAULT_PROPERTY_VALUE(true));
        ENTT_PROPERTY_EX_REFL(ENTT_MEMBER_ACCESSORS(set_allow_merging, get_allow_merging), "allow_merging", group_prop, ENTT_DEFAULT_PROPERTY_VALUE(true));
        ENTT_PROPERTY_EX_REFL(ENTT_MEMBER_ACCESSORS(set_portal_autoplace_priority, get_portal_autoplace_priority),
                "autoplace_priority",
                group_prop, ENTT_PROPERTY_RANGE(-16, 16, 1), ENTT_DEFAULT_PROPERTY_VALUE(0));
        ENTT_ADD_EDITOR_FUNCS();
    }
}
static void _unbind_methods() {
    entt::meta_reset<CullInstanceComponent::PortalMode>();
    entt::meta_reset<CullInstanceComponent>();
}

static const ComponentOperations ops = {
    _bind_methods,
    _unbind_methods
};
const ComponentOperations * get_cull_instance_operations() {
    return &ops;
}
/*
 void VisibilityNotifier3D::_refresh_portal_mode() {
    // only create in the visual server if we are roaming.
    // All other cases don't require a visual server rep.
    // Global and ignore are the same (existing client side functionality only).
    // Static and dynamic require only a one off creation at conversion.
    if (get_portal_mode() == CullInstanceComponent::PORTAL_MODE_ROAMING) {
        if (is_inside_world()) {
            if (_cull_instance_rid == RID()) {
                _cull_instance_rid = RID_PRIME(VisualServer::get_singleton()->ghost_create());
            }

            if (is_inside_world() && get_world().is_valid() && get_world()->get_scenario().is_valid() && is_inside_tree()) {
                AABB world_aabb = get_global_transform().xform(aabb);
                VisualServer::get_singleton()->ghost_set_scenario(_cull_instance_rid, get_world()->get_scenario(), get_instance_id(), world_aabb);
            }
        } else {
            if (_cull_instance_rid != RID()) {
                VisualServer::get_singleton()->free(_cull_instance_rid);
                _cull_instance_rid = RID();
            }
        }

    } else {
        if (_cull_instance_rid != RID()) {
            VisualServer::get_singleton()->free(_cull_instance_rid);
            _cull_instance_rid = RID();
        }
    }
}

 *
 */
