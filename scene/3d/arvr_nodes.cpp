/*************************************************************************/
/*  arvr_nodes.cpp                                                       */
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

#include "arvr_nodes.h"

#include "core/method_bind.h"
#include "core/os/input.h"
#include "core/translation_helpers.h"
#include "servers/arvr/arvr_interface.h"
#include "servers/arvr_server.h"

IMPL_GDCLASS(ARVRCamera)
IMPL_GDCLASS(ARVRController)
IMPL_GDCLASS(ARVRAnchor)
IMPL_GDCLASS(ARVROrigin)

////////////////////////////////////////////////////////////////////////////////////////////////////

void ARVRCamera::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            // need to find our ARVROrigin parent and let it know we're its camera!
            ARVROrigin *origin = object_cast<ARVROrigin>(get_parent());
            if (origin != nullptr) {
                origin->set_tracked_camera(this);
            }
        } break;
        case NOTIFICATION_EXIT_TREE: {
            // need to find our ARVROrigin parent and let it know we're no longer its camera!
            ARVROrigin *origin = object_cast<ARVROrigin>(get_parent());
            if (origin != nullptr) {
                origin->clear_tracked_camera_if(this);
            }
        } break;
    }
}

String ARVRCamera::get_configuration_warning() const {
    if (!is_visible() || !is_inside_tree())
        return String();

    String warning = BaseClassName::get_configuration_warning();

    // must be child node of ARVROrigin!
    ARVROrigin *origin = object_cast<ARVROrigin>(get_parent());
    if (origin == nullptr) {
        if(!warning.empty())
            warning += "\n\n";
        return warning + TTR("ARVRCamera must have an ARVROrigin node as its parent.");
    }

    return String();
}

Vector3 ARVRCamera::project_local_ray_normal(const Point2 &p_pos) const {
    // get our ARVRServer
    ARVRServer *arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL_V(arvr_server, Vector3());

    Ref<ARVRInterface> arvr_interface = arvr_server->get_primary_interface();
    if (not arvr_interface) {
        // we might be in the editor or have VR turned off, just call superclass
        return Camera3D::project_local_ray_normal(p_pos);
    }

    ERR_FAIL_COND_V_MSG(!is_inside_tree(), Vector3(), "Camera3D is not inside scene.");

    Size2 viewport_size = get_viewport()->get_camera_rect_size();
    Vector2 cpos = get_viewport()->get_camera_coords(p_pos);
    Vector3 ray;

    CameraMatrix cm = arvr_interface->get_projection_for_eye(ARVREyes::EYE_MONO, viewport_size.aspect(), get_znear(), get_zfar());

    Vector2 screen_he = cm.get_viewport_half_extents();
    ray = Vector3(((cpos.x / viewport_size.width) * 2.0 - 1.0) * screen_he.x, ((1.0 - (cpos.y / viewport_size.height)) * 2.0 - 1.0) * screen_he.y, -get_znear()).normalized();

    return ray;
}

Point2 ARVRCamera::unproject_position(const Vector3 &p_pos) const {
    // get our ARVRServer
    ARVRServer *arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL_V(arvr_server, Vector2());

    Ref<ARVRInterface> arvr_interface = arvr_server->get_primary_interface();
    if (not arvr_interface) {
        // we might be in the editor or have VR turned off, just call superclass
        return Camera3D::unproject_position(p_pos);
    }

    ERR_FAIL_COND_V_MSG(!is_inside_tree(), Vector2(), "Camera3D is not inside scene.");

    Size2 viewport_size = get_viewport()->get_visible_rect().size;

    CameraMatrix cm = arvr_interface->get_projection_for_eye(ARVREyes::EYE_MONO, viewport_size.aspect(), get_znear(), get_zfar());

    Plane p(get_camera_transform().xform_inv(p_pos), 1.0);

    p = cm.xform4(p);
    p.normal /= p.d;

    Point2 res;
    res.x = (p.normal.x * 0.5 + 0.5) * viewport_size.x;
    res.y = (-p.normal.y * 0.5 + 0.5) * viewport_size.y;

    return res;
}

Vector3 ARVRCamera::project_position(const Point2 &p_point, float p_z_depth) const {
    // get our ARVRServer
    ARVRServer *arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL_V(arvr_server, Vector3());

    Ref<ARVRInterface> arvr_interface = arvr_server->get_primary_interface();
    if (not arvr_interface) {
        // we might be in the editor or have VR turned off, just call superclass
        return Camera3D::project_position(p_point, p_z_depth);
    }

    ERR_FAIL_COND_V_MSG(!is_inside_tree(), Vector3(), "Camera3D is not inside scene.");

    Size2 viewport_size = get_viewport()->get_visible_rect().size;

    CameraMatrix cm = arvr_interface->get_projection_for_eye(ARVREyes::EYE_MONO, viewport_size.aspect(), get_znear(), get_zfar());

    Vector2 vp_he = cm.get_viewport_half_extents();

    Vector2 point;
    point.x = (p_point.x / viewport_size.x) * 2.0 - 1.0;
    point.y = (1.0 - (p_point.y / viewport_size.y)) * 2.0 - 1.0;
    point *= vp_he;

    Vector3 p(point.x, point.y, -p_z_depth);

    return get_camera_transform().xform(p);
}

Frustum ARVRCamera::get_frustum() const {
    // get our ARVRServer
    ARVRServer *arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL_V(arvr_server, Frustum());

    Ref<ARVRInterface> arvr_interface = arvr_server->get_primary_interface();
    if (not arvr_interface) {
        // we might be in the editor or have VR turned off, just call superclass
        return Camera3D::get_frustum();
    }

    ERR_FAIL_COND_V(!is_inside_world(), Frustum());

    Size2 viewport_size = get_viewport()->get_visible_rect().size;
    CameraMatrix cm = arvr_interface->get_projection_for_eye(ARVREyes::EYE_MONO, viewport_size.aspect(), get_znear(), get_zfar());
    return cm.get_projection_planes(get_camera_transform());
}

ARVRCamera::ARVRCamera(){
    // nothing to do here yet for now..
}

ARVRCamera::~ARVRCamera(){
    // nothing to do here yet for now..
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ARVRController::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            set_process_internal(true);
        }; break;
        case NOTIFICATION_EXIT_TREE: {
            set_process_internal(false);
        }; break;
        case NOTIFICATION_INTERNAL_PROCESS: {
            // get our ARVRServer
            ARVRServer *arvr_server = ARVRServer::get_singleton();
            ERR_FAIL_NULL(arvr_server);

            // find the tracker for our controller
            auto tracker = arvr_server->find_by_type_and_id(ARVRServer::TRACKER_CONTROLLER, controller_id);
            if (!tracker) {
                // this controller is currently turned off
                is_active = false;
                button_states = 0;
            } else {
                is_active = true;
                set_transform(tracker->get_transform(true));

                int joy_id = tracker->get_joy_id();
                if (joy_id >= 0) {
                    int mask = 1;
                    // check button states
                    for (int i = 0; i < 16; i++) {
                        bool was_pressed = (button_states & mask) == mask;
                        bool is_pressed = Input::get_singleton()->is_joy_button_pressed(joy_id, i);

                        if (!was_pressed && is_pressed) {
                            emit_signal("button_pressed", i);
                            button_states += mask;
                        } else if (was_pressed && !is_pressed) {
                            emit_signal("button_release", i);
                            button_states -= mask;
                        }

                        mask = mask << 1;
                    }

                } else {
                    button_states = 0;
                }

                // check for an updated mesh
                Ref<Mesh> trackerMesh = tracker->get_mesh();
                if (mesh != trackerMesh) {
                    mesh = trackerMesh;
                    emit_signal("mesh_updated", mesh);
                }
            }
        } break;
        default:
            break;
    }
}

void ARVRController::_bind_methods() {
    SE_BIND_METHOD(ARVRController,set_controller_id);
    SE_BIND_METHOD(ARVRController,get_controller_id);
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "controller_id", PropertyHint::Range, "0,32,1"), "set_controller_id", "get_controller_id");
    SE_BIND_METHOD(ARVRController,get_controller_name);

    // passthroughs to information about our related joystick
    SE_BIND_METHOD(ARVRController,get_joystick_id);
    SE_BIND_METHOD(ARVRController,is_button_pressed);
    SE_BIND_METHOD(ARVRController,get_joystick_axis);

    SE_BIND_METHOD(ARVRController,get_is_active);
    SE_BIND_METHOD(ARVRController,get_hand);

    SE_BIND_METHOD(ARVRController,get_rumble);
    SE_BIND_METHOD(ARVRController,set_rumble);
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "rumble", PropertyHint::Range, "0.0,1.0,0.01"), "set_rumble", "get_rumble");
    ADD_PROPERTY_DEFAULT("rumble", 0.0);

    SE_BIND_METHOD(ARVRController,get_mesh);

    ADD_SIGNAL(MethodInfo("button_pressed", PropertyInfo(VariantType::INT, "button")));
    ADD_SIGNAL(MethodInfo("button_release", PropertyInfo(VariantType::INT, "button")));
    ADD_SIGNAL(MethodInfo("mesh_updated", PropertyInfo(VariantType::OBJECT, "mesh", PropertyHint::ResourceType, "Mesh")));
}

void ARVRController::set_controller_id(int p_controller_id) {
    // We don't check any bounds here, this controller may not yet be active and just be a place holder until it is.
    // Note that setting this to 0 means this node is not bound to a controller yet.
    controller_id = p_controller_id;
    update_configuration_warning();
}

int ARVRController::get_controller_id() const {
    return controller_id;
}

StringName ARVRController::get_controller_name() const {
    // get our ARVRServer
    ARVRServer *arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL_V(arvr_server, StringName());

    auto tracker = arvr_server->find_by_type_and_id(ARVRServer::TRACKER_CONTROLLER, controller_id);
    if (tracker == nullptr) {
        return StringName("Not connected");
    }

    return tracker->get_name();
}

int ARVRController::get_joystick_id() const {
    // get our ARVRServer
    ARVRServer *arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL_V(arvr_server, 0);

    auto tracker = arvr_server->find_by_type_and_id(ARVRServer::TRACKER_CONTROLLER, controller_id);
    if (tracker == nullptr) {
        // No tracker? no joystick id... (0 is our first joystick)
        return -1;
    }

    return tracker->get_joy_id();
}

int ARVRController::is_button_pressed(int p_button) const {
    int joy_id = get_joystick_id();
    if (joy_id == -1) {
        return false;
    }

    return Input::get_singleton()->is_joy_button_pressed(joy_id, p_button);
}

float ARVRController::get_joystick_axis(int p_axis) const {
    int joy_id = get_joystick_id();
    if (joy_id == -1) {
        return 0.0;
    }

    return Input::get_singleton()->get_joy_axis(joy_id, p_axis);
}

real_t ARVRController::get_rumble() const {
    // get our ARVRServer
    ARVRServer *arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL_V(arvr_server, 0.0);

    auto tracker = arvr_server->find_by_type_and_id(ARVRServer::TRACKER_CONTROLLER, controller_id);
    if (tracker == nullptr) {
        return 0.0;
    }

    return tracker->get_rumble();
}

void ARVRController::set_rumble(float p_rumble) {
    // get our ARVRServer
    ARVRServer *arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL(arvr_server);

    auto tracker = arvr_server->find_by_type_and_id(ARVRServer::TRACKER_CONTROLLER, controller_id);
    if (tracker != nullptr) {
        tracker->set_rumble(p_rumble);
    }
}

Ref<Mesh> ARVRController::get_mesh() const {
    return mesh;
}

bool ARVRController::get_is_active() const {
    return is_active;
}

ARVRPositionalTracker::TrackerHand ARVRController::get_hand() const {
    // get our ARVRServer
    ARVRServer *arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL_V(arvr_server, ARVRPositionalTracker::TRACKER_HAND_UNKNOWN);

    auto tracker = arvr_server->find_by_type_and_id(ARVRServer::TRACKER_CONTROLLER, controller_id);
    if (tracker == nullptr) {
        return ARVRPositionalTracker::TRACKER_HAND_UNKNOWN;
    }

    return tracker->get_hand();
}

String ARVRController::get_configuration_warning() const {
    if (!is_visible() || !is_inside_tree())
        return String();

    String warning = BaseClassName::get_configuration_warning();
    // must be child node of ARVROrigin!
    ARVROrigin *origin = object_cast<ARVROrigin>(get_parent());
    if (origin == nullptr) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("ARVRController must have an ARVROrigin node as its parent.");
    };

    if (controller_id == 0) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("The controller ID must not be 0 or this controller won't be bound to an actual controller.");
    }

    return warning;
}

ARVRController::ARVRController() {
    controller_id = 1;
    is_active = true;
    button_states = 0;
}

ARVRController::~ARVRController(){
    // nothing to do here yet for now..
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ARVRAnchor::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            set_process_internal(true);
        } break;
        case NOTIFICATION_EXIT_TREE: {
            set_process_internal(false);
        } break;
        case NOTIFICATION_INTERNAL_PROCESS: {
            // get our ARVRServer
            ARVRServer *arvr_server = ARVRServer::get_singleton();
            ERR_FAIL_NULL(arvr_server);

            // find the tracker for our anchor
            auto tracker = arvr_server->find_by_type_and_id(ARVRServer::TRACKER_ANCHOR, anchor_id);
            if (tracker == nullptr) {
                // this anchor is currently not available
                is_active = false;
            } else {
                is_active = true;
                Transform transform;

                // we'll need our world_scale
                real_t world_scale = arvr_server->get_world_scale();

                // get our info from our tracker
                transform.basis = tracker->get_orientation();
                transform.origin = tracker->get_position(); // <-- already adjusted to world scale

                // our basis is scaled to the size of the plane the anchor is tracking
                // extract the size from our basis and reset the scale
                size = transform.basis.get_scale() * world_scale;
                transform.basis.orthonormalize();

                // apply our reference frame and set our transform
                set_transform(arvr_server->get_reference_frame() * transform);

                // check for an updated mesh
                Ref<Mesh> trackerMesh = tracker->get_mesh();
                if (mesh != trackerMesh) {
                    mesh = trackerMesh;
                    emit_signal("mesh_updated", mesh);
                }
            }
        } break;
        default:
            break;
    }
}

void ARVRAnchor::_bind_methods() {

    SE_BIND_METHOD(ARVRAnchor,set_anchor_id);
    SE_BIND_METHOD(ARVRAnchor,get_anchor_id);
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "anchor_id", PropertyHint::Range, "0,32,1"), "set_anchor_id", "get_anchor_id");
    SE_BIND_METHOD(ARVRAnchor,get_anchor_name);

    SE_BIND_METHOD(ARVRAnchor,get_is_active);
    SE_BIND_METHOD(ARVRAnchor,get_size);

    SE_BIND_METHOD(ARVRAnchor,get_plane);

    SE_BIND_METHOD(ARVRAnchor,get_mesh);
    ADD_SIGNAL(MethodInfo("mesh_updated", PropertyInfo(VariantType::OBJECT, "mesh", PropertyHint::ResourceType, "Mesh")));
}

void ARVRAnchor::set_anchor_id(int p_anchor_id) {
    // We don't check any bounds here, this anchor may not yet be active and just be a place holder until it is.
    // Note that setting this to 0 means this node is not bound to an anchor yet.
    anchor_id = p_anchor_id;
    update_configuration_warning();
}

int ARVRAnchor::get_anchor_id() const {
    return anchor_id;
}

Vector3 ARVRAnchor::get_size() const {
    return size;
}

StringName ARVRAnchor::get_anchor_name() const {
    // get our ARVRServer
    ARVRServer *arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL_V(arvr_server, StringName());

    auto tracker = arvr_server->find_by_type_and_id(ARVRServer::TRACKER_ANCHOR, anchor_id);
    if (tracker == nullptr) {
        return StringName("Not connected");
    }

    return tracker->get_name();
}

bool ARVRAnchor::get_is_active() const {
    return is_active;
}

String ARVRAnchor::get_configuration_warning() const {
    if (!is_visible() || !is_inside_tree())
        return String();

    String warning = BaseClassName::get_configuration_warning();
    // must be child node of ARVROrigin!
    ARVROrigin *origin = object_cast<ARVROrigin>(get_parent());
    if (origin == nullptr) {
        if (warning != String()) {
            warning += "\n\n";
        }
        warning += TTR("ARVRAnchor must have an ARVROrigin node as its parent.");
    };

    if (anchor_id == 0) {
        if (warning != String()) {
            warning += "\n\n";
        }
        warning += TTR("The anchor ID must not be 0 or this anchor won't be bound to an actual anchor.");
    }

    return warning;
}

Plane ARVRAnchor::get_plane() const {
    Vector3 location = get_translation();
    Basis orientation = get_transform().basis;

    Plane plane(location, orientation.get_axis(1).normalized());

    return plane;
}

Ref<Mesh> ARVRAnchor::get_mesh() const {
    return mesh;
}

ARVRAnchor::ARVRAnchor() {
    anchor_id = 1;
    is_active = true;
}

ARVRAnchor::~ARVRAnchor(){
    // nothing to do here yet for now..
}

////////////////////////////////////////////////////////////////////////////////////////////////////

String ARVROrigin::get_configuration_warning() const {
    if (!is_visible() || !is_inside_tree())
        return String();

    String warning = BaseClassName::get_configuration_warning();
    if (tracked_camera == nullptr) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("ARVROrigin requires an ARVRCamera child node.");
    }

    return warning;
}

void ARVROrigin::_bind_methods() {
    SE_BIND_METHOD(ARVROrigin,set_world_scale);
    SE_BIND_METHOD(ARVROrigin,get_world_scale);
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "world_scale"), "set_world_scale", "get_world_scale");
}

void ARVROrigin::set_tracked_camera(ARVRCamera *p_tracked_camera) {
    tracked_camera = p_tracked_camera;
}

void ARVROrigin::clear_tracked_camera_if(ARVRCamera *p_tracked_camera) {
    if (tracked_camera == p_tracked_camera) {
        tracked_camera = nullptr;
    }
}

float ARVROrigin::get_world_scale() const {
    // get our ARVRServer
    ARVRServer *arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL_V(arvr_server, 1.0);

    return arvr_server->get_world_scale();
}

void ARVROrigin::set_world_scale(float p_world_scale) {
    // get our ARVRServer
    ARVRServer *arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL(arvr_server);

    arvr_server->set_world_scale(p_world_scale);
}

void ARVROrigin::_notification(int p_what) {
    // get our ARVRServer
    ARVRServer *arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL(arvr_server);

    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            set_process_internal(true);
        }; break;
        case NOTIFICATION_EXIT_TREE: {
            set_process_internal(false);
        }; break;
        case NOTIFICATION_INTERNAL_PROCESS: {
            // set our world origin to our node transform
            arvr_server->set_world_origin(get_global_transform());

            // check if we have a primary interface
            Ref<ARVRInterface> arvr_interface = arvr_server->get_primary_interface();
            if (arvr_interface && tracked_camera != nullptr) {
                // get our positioning transform for our headset
                Transform t = arvr_interface->get_transform_for_eye(ARVREyes::EYE_MONO, Transform());

                // now apply this to our camera
                tracked_camera->set_transform(t);
            }
        } break;
        default:
            break;
    }

    // send our notification to all active ARVR interfaces, they may need to react to it also
    for (int i = 0; i < arvr_server->get_interface_count(); i++) {
        Ref<ARVRInterface> interface = arvr_server->get_interface(i);
        if (interface && interface->is_initialized()) {
            interface->notification(p_what);
        }
    }
}

ARVROrigin::ARVROrigin() {
    tracked_camera = nullptr;
}

ARVROrigin::~ARVROrigin(){
    // nothing to do here yet for now..
}
