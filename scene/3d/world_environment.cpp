/*************************************************************************/
/*  world_environment.cpp                                                */
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

#include "world_environment.h"
#include "scene/main/viewport.h"
#include "core/method_bind.h"
#include "core/translation_helpers.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/environment.h"
#include "scene/resources/world_3d.h"

IMPL_GDCLASS(WorldEnvironment)

void WorldEnvironment::_notification(int p_what) {

    if (p_what == Node3D::NOTIFICATION_ENTER_WORLD || p_what == Node3D::NOTIFICATION_ENTER_TREE) {

        if (environment) {
            if (get_viewport()->find_world_3d()->get_environment()) {
                WARN_PRINT("World already has an environment (Another WorldEnvironment?), overriding.");
            }
            get_viewport()->find_world_3d()->set_environment(environment);
            add_to_group(StringName("_world_environment_" + itos(entt::to_integral(get_viewport()->find_world_3d()->get_scenario()))));
        }

    } else if (p_what == Node3D::NOTIFICATION_EXIT_WORLD || p_what == Node3D::NOTIFICATION_EXIT_TREE) {

        if (environment && get_viewport()->find_world_3d()->get_environment() == environment) {
            get_viewport()->find_world_3d()->set_environment(Ref<Environment>());
            remove_from_group(StringName("_world_environment_" + itos(entt::to_integral(get_viewport()->find_world_3d()->get_scenario()))));
        }
    }
}

void WorldEnvironment::set_environment(const Ref<Environment> &p_environment) {

    if (is_inside_tree() && environment && get_viewport()->find_world_3d()->get_environment() == environment) {
        get_viewport()->find_world_3d()->set_environment(Ref<Environment>());
        remove_from_group(StringName("_world_environment_" + itos(entt::to_integral(get_viewport()->find_world_3d()->get_scenario()))));
        //clean up
    }

    environment = p_environment;
    if (is_inside_tree() && environment) {
        if (get_viewport()->find_world_3d()->get_environment()) {
            WARN_PRINT("World already has an environment (Another WorldEnvironment?), overriding.");
        }
        get_viewport()->find_world_3d()->set_environment(environment);
        add_to_group(StringName("_world_environment_" + itos(entt::to_integral(get_viewport()->find_world_3d()->get_scenario()))));
    }

    update_configuration_warning();
}

Ref<Environment> WorldEnvironment::get_environment() const {

    return environment;
}

String WorldEnvironment::get_configuration_warning() const {

    String warning = Node::get_configuration_warning();
    if (!environment) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("WorldEnvironment requires its \"Environment\" property to contain an Environment to have a visible effect.");
        return warning;
    }

    if (/*!is_visible_in_tree() ||*/ !is_inside_tree())
        return String();

    Dequeue<Node *> nodes;
    get_tree()->get_nodes_in_group(StringName("_world_environment_" + itos(entt::to_integral(get_viewport()->find_world_3d()->get_scenario()))), &nodes);

    if (nodes.size() > 1) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("Only one WorldEnvironment is allowed per scene (or set of instanced scenes).");
    }

    // Commenting this warning for now, I think it makes no sense. If anyone can figure out what its supposed to do, feedback welcome. Else it should be deprecated.
    //if (environment.is_valid() && get_viewport() && !get_viewport()->get_camera() && environment->get_background() != Environment::BG_CANVAS) {
    //  return TTR("This WorldEnvironment is ignored. Either add a Camera (for 3D scenes) or set this environment's Background Mode to Canvas (for 2D scenes).");
    //}

    return warning;
}

void WorldEnvironment::_bind_methods() {

    SE_BIND_METHOD(WorldEnvironment,set_environment);
    SE_BIND_METHOD(WorldEnvironment,get_environment);
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "environment", PropertyHint::ResourceType, "Environment"), "set_environment", "get_environment");
}

WorldEnvironment::WorldEnvironment() {
}
