/*************************************************************************/
/*  viewport.cpp                                                         */
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

#include "viewport.h"

#include "core/core_string_names.h"
#include "core/debugger/script_debugger.h"
#include "core/method_bind.h"
#include "core/object_db.h"
#include "core/os/input.h"
#include "core/script_language.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/translation_helpers.h"
#include "EASTL/sort.h"
#include "scene/2d/collision_object_2d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/collision_object_3d.h"
#include "scene/3d/listener_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/popup_menu.h"
#include "scene/resources/style_box.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/scene_tree.h"
#include "scene/main/timer.h"
#include "scene/resources/mesh.h"
#include "scene/scene_string_names.h"
#include "servers/physics_server_2d.h"

IMPL_GDCLASS(ViewportTexture)
IMPL_GDCLASS(Viewport)

VARIANT_ENUM_CAST(Viewport::UpdateMode);
VARIANT_ENUM_CAST(Viewport::ShadowAtlasQuadrantSubdiv);
VARIANT_ENUM_CAST(Viewport::MSAA);
VARIANT_ENUM_CAST(Viewport::Usage);
VARIANT_ENUM_CAST(Viewport::DebugDraw);
VARIANT_ENUM_CAST(Viewport::ClearMode);
VARIANT_ENUM_CAST(Viewport::RenderInfo);

void ViewportTexture::setup_local_to_scene() {

    if (vp) {
        vp->viewport_textures.erase(this);
    }

    vp = nullptr;

    Node *local_scene = get_local_scene();
    if (!local_scene) {
        return;
    }

    Node *vpn = local_scene->get_node(path);
    ERR_FAIL_COND_MSG(!vpn, "ViewportTexture: Path to node is invalid.");

    vp = object_cast<Viewport>(vpn);

    ERR_FAIL_COND_MSG(!vp, "ViewportTexture: Path to node does not point to a viewport.");

    vp->viewport_textures.insert(this);

    RenderingServer::get_singleton()->texture_set_proxy(proxy, vp->texture_rid);

    vp->texture_flags = flags;
    RenderingServer::get_singleton()->texture_set_flags(vp->texture_rid, flags);
}

void ViewportTexture::set_viewport_path_in_scene(const NodePath &p_path) {

    if (path == p_path)
        return;

    path = p_path;

    if (get_local_scene()) {
        setup_local_to_scene();
    }
}

NodePath ViewportTexture::get_viewport_path_in_scene() const {

    return path;
}

int ViewportTexture::get_width() const {

    ERR_FAIL_COND_V(!vp, 0);
    return vp->size.width;
}
int ViewportTexture::get_height() const {

    ERR_FAIL_COND_V(!vp, 0);
    return vp->size.height;
}
Size2 ViewportTexture::get_size() const {

    ERR_FAIL_COND_V(!vp, Size2());
    return vp->size;
}
RID ViewportTexture::get_rid() const {

    //ERR_FAIL_COND_V(!vp, RID());
    return proxy;
}

bool ViewportTexture::has_alpha() const {

    return false;
}
Ref<Image> ViewportTexture::get_data() const {

    ERR_FAIL_COND_V(!vp, Ref<Image>());
    return RenderingServer::get_singleton()->texture_get_data(vp->texture_rid);
}
void ViewportTexture::set_flags(uint32_t p_flags) {
    flags = p_flags;

    if (!vp)
        return;

    vp->texture_flags = flags;
    RenderingServer::get_singleton()->texture_set_flags(vp->texture_rid, flags);
}

uint32_t ViewportTexture::get_flags() const {

    return flags;
}

void ViewportTexture::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_viewport_path_in_scene", {"path"}), &ViewportTexture::set_viewport_path_in_scene);
    MethodBinder::bind_method(D_METHOD("get_viewport_path_in_scene"), &ViewportTexture::get_viewport_path_in_scene);

    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "viewport_path", PropertyHint::NodePathValidTypes, "Viewport", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NODE_PATH_FROM_SCENE_ROOT), "set_viewport_path_in_scene", "get_viewport_path_in_scene");
}

ViewportTexture::ViewportTexture() {

    vp = nullptr;
    flags = 0;
    set_local_to_scene(true);
    proxy = RenderingServer::get_singleton()->texture_create();
}

ViewportTexture::~ViewportTexture() {

    if (vp) {
        vp->viewport_textures.erase(this);
    }

    RenderingServer::get_singleton()->free_rid(proxy);
}

/////////////////////////////////////

class TooltipPanel : public PanelContainer {

    GDCLASS(TooltipPanel,PanelContainer)


public:
    TooltipPanel(){}
};
IMPL_GDCLASS(TooltipPanel)
class TooltipLabel : public Label {

    GDCLASS(TooltipLabel,Label)
public:
    TooltipLabel(){}

};
IMPL_GDCLASS(TooltipLabel)
void register_viewport_local_classes()
{
    TooltipPanel::initialize_class();
    TooltipLabel::initialize_class();
}
Viewport::GUI::GUI() {

    dragging = false;
    mouse_focus = nullptr;
    mouse_click_grabber = nullptr;
    mouse_focus_mask = 0;
    key_focus = nullptr;
    mouse_over = nullptr;

    tooltip = nullptr;
    tooltip_popup = nullptr;
    tooltip_label = nullptr;
    subwindow_visibility_dirty = false;
    subwindow_order_dirty = false;
}

/////////////////////////////////////

void Viewport::_update_stretch_transform() {

    if (size_override_stretch && size_override) {

        stretch_transform = Transform2D();
        Size2 scale = size / (size_override_size + size_override_margin * 2);
        stretch_transform.scale(scale);
        stretch_transform.elements[2] = size_override_margin * scale;

    } else {

        stretch_transform = Transform2D();
    }

    _update_global_transform();
}

void Viewport::update_worlds() {

    if (!is_inside_tree())
        return;

    Rect2 abstracted_rect = Rect2(Vector2(), get_visible_rect().size);
    Rect2 xformed_rect = (global_canvas_transform * canvas_transform).affine_inverse().xform(abstracted_rect);
    find_world_2d()->_update_viewport(this, xformed_rect);
    find_world_2d()->_update();

    find_world()->_update(get_tree()->get_frame());
}

void Viewport::_collision_object_input_event(CollisionObject3D *p_object, Camera3D *p_camera, const Ref<InputEvent> &p_input_event, const Vector3 &p_pos, const Vector3 &p_normal, int p_shape) {

    Transform object_transform = p_object->get_global_transform();
    Transform camera_transform = p_camera->get_global_transform();
    ObjectID id = p_object->get_instance_id();

    //avoid sending the fake event unnecessarily if nothing really changed in the context
    if (object_transform == physics_last_object_transform && camera_transform == physics_last_camera_transform && physics_last_id == id) {
        Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_input_event);
        if (mm && mm->get_device() == InputEvent::DEVICE_ID_INTERNAL) {
            return; //discarded
        }
    }
    p_object->_input_event(camera, p_input_event, p_pos, p_normal, p_shape);
    physics_last_object_transform = object_transform;
    physics_last_camera_transform = camera_transform;
    physics_last_id = id;
}
void Viewport::_own_world_changed() {
    ERR_FAIL_COND(not world);
    ERR_FAIL_COND(not own_world);

    if (is_inside_tree()) {
        _propagate_exit_world(this);
    }

    own_world = dynamic_ref_cast<World3D>(world->duplicate());

    if (is_inside_tree()) {
        _propagate_enter_world(this);
    }

    if (is_inside_tree()) {
        RenderingServer::get_singleton()->viewport_set_scenario(viewport, find_world()->get_scenario());
    }

    _update_listener();
}
void Viewport::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_TREE: {

            if (get_parent()) {
                parent = get_parent()->get_viewport();
                RenderingServer::get_singleton()->viewport_set_parent_viewport(viewport, parent->get_viewport_rid());
            } else {
                parent = nullptr;
            }

            current_canvas = find_world_2d()->get_canvas();
            RenderingServer::get_singleton()->viewport_set_scenario(viewport, find_world()->get_scenario());
            RenderingServer::get_singleton()->viewport_attach_canvas(viewport, current_canvas);

            _update_listener();
            _update_listener_2d();

            find_world_2d()->_register_viewport(this, Rect2());

            add_to_group("_viewports");
            if (get_tree()->is_debugging_collisions_hint()) {
                //2D
                PhysicsServer2D::get_singleton()->space_set_debug_contacts(find_world_2d()->get_space(), get_tree()->get_collision_debug_contact_count());
                contact_2d_debug = RenderingServer::get_singleton()->canvas_item_create();
                RenderingServer::get_singleton()->canvas_item_set_parent(contact_2d_debug, find_world_2d()->get_canvas());
                //3D
                PhysicsServer3D::get_singleton()->space_set_debug_contacts(find_world()->get_space(), get_tree()->get_collision_debug_contact_count());
                contact_3d_debug_multimesh = RenderingServer::get_singleton()->multimesh_create();
                RenderingServer::get_singleton()->multimesh_allocate(contact_3d_debug_multimesh, get_tree()->get_collision_debug_contact_count(), RS::MULTIMESH_TRANSFORM_3D, RS::MULTIMESH_COLOR_8BIT);
                RenderingServer::get_singleton()->multimesh_set_visible_instances(contact_3d_debug_multimesh, 0);
                RenderingServer::get_singleton()->multimesh_set_mesh(contact_3d_debug_multimesh, get_tree()->get_debug_contact_mesh()->get_rid());
                contact_3d_debug_instance = RenderingServer::get_singleton()->instance_create();
                RenderingServer::get_singleton()->instance_set_base(contact_3d_debug_instance, contact_3d_debug_multimesh);
                RenderingServer::get_singleton()->instance_set_scenario(contact_3d_debug_instance, find_world()->get_scenario());
                //RenderingServer::get_singleton()->instance_geometry_set_flag(contact_3d_debug_instance, RS::INSTANCE_FLAG_VISIBLE_IN_ALL_ROOMS, true);
            }

            RenderingServer::get_singleton()->viewport_set_active(viewport, true);
        } break;
        case NOTIFICATION_READY: {
#ifndef _3D_DISABLED
            if (!listeners.empty() && !listener) {
                Listener3D *first = nullptr;
                for (Listener3D * E : listeners) {

                    if (first == nullptr || first->is_greater_than(E)) {
                        first = E;
                    }
                }

                if (first)
                    first->make_current();
            }

            if (!cameras.empty() && !camera) {
                //there are cameras but no current camera, pick first in tree and make it current
                Camera3D *first = nullptr;
                for (Camera3D * E : cameras) {

                    if (first == nullptr || first->is_greater_than(E)) {
                        first = E;
                    }
                }

                if (first)
                    first->make_current();
            }
#endif

            // Enable processing for tooltips, collision debugging, physics object picking, etc.
            set_process_internal(true);
            set_physics_process_internal(true);

        } break;
        case NOTIFICATION_EXIT_TREE: {

            _gui_cancel_tooltip();
            if (world_2d)
                world_2d->_remove_viewport(this);

            RenderingServer::get_singleton()->viewport_set_scenario(viewport, RID());
            //			SpatialSoundServer::get_singleton()->listener_set_space(internal_listener, RID());
            RenderingServer::get_singleton()->viewport_remove_canvas(viewport, current_canvas);
            if (contact_2d_debug.is_valid()) {
                RenderingServer::get_singleton()->free_rid(contact_2d_debug);
                contact_2d_debug = RID();
            }

            if (contact_3d_debug_multimesh.is_valid()) {
                RenderingServer::get_singleton()->free_rid(contact_3d_debug_multimesh);
                RenderingServer::get_singleton()->free_rid(contact_3d_debug_instance);
                contact_3d_debug_instance = RID();
                contact_3d_debug_multimesh = RID();
            }

            remove_from_group("_viewports");

            RenderingServer::get_singleton()->viewport_set_active(viewport, false);

        } break;
        case NOTIFICATION_INTERNAL_PROCESS: {

            if (gui.tooltip_timer >= 0) {
                gui.tooltip_timer -= get_process_delta_time();
                if (gui.tooltip_timer < 0) {
                    _gui_show_tooltip();
                }
            }

        } break;
        case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {

            if (get_tree()->is_debugging_collisions_hint() && contact_2d_debug.is_valid()) {

                RenderingServer::get_singleton()->canvas_item_clear(contact_2d_debug);
                RenderingServer::get_singleton()->canvas_item_set_draw_index(contact_2d_debug, 0xFFFFF); //very high index

                const Vector<Vector2> &points = PhysicsServer2D::get_singleton()->space_get_contacts(find_world_2d()->get_space());
                int point_count = PhysicsServer2D::get_singleton()->space_get_contact_count(find_world_2d()->get_space());
                Color ccol = get_tree()->get_debug_collision_contact_color();

                for (int i = 0; i < point_count; i++) {

                    RenderingServer::get_singleton()->canvas_item_add_rect(contact_2d_debug, Rect2(points[i] - Vector2(2, 2), Vector2(5, 5)), ccol);
                }
            }

            if (get_tree()->is_debugging_collisions_hint() && contact_3d_debug_multimesh.is_valid()) {

                const Vector<Vector3> & points = PhysicsServer3D::get_singleton()->space_get_contacts(find_world()->get_space());
                int point_count = PhysicsServer3D::get_singleton()->space_get_contact_count(find_world()->get_space());

                RenderingServer::get_singleton()->multimesh_set_visible_instances(contact_3d_debug_multimesh, point_count);
            }

            if (physics_object_picking && (to_screen_rect == Rect2() || Input::get_singleton()->get_mouse_mode() != Input::MOUSE_MODE_CAPTURED)) {

#ifndef _3D_DISABLED
                Vector2 last_pos(1e20f, 1e20f);
                CollisionObject3D *last_object = nullptr;
                ObjectID last_id = ObjectID(0ULL);
#endif
                PhysicsDirectSpaceState3D::RayResult result;
                PhysicsDirectSpaceState2D *ss2d = PhysicsServer2D::get_singleton()->space_get_direct_state(find_world_2d()->get_space());

                if (physics_has_last_mousepos) {
                    // if no mouse event exists, create a motion one. This is necessary because objects or camera may have moved.
                    // while this extra event is sent, it is checked if both camera and last object and last ID did not move. If nothing changed, the event is discarded to avoid flooding with unnecessary motion events every frame
                    bool has_mouse_event = false;
                    for (const Ref<InputEvent> &E : physics_picking_events) {
                        Ref<InputEventMouse> m = dynamic_ref_cast<InputEventMouse>(E);
                        if (m) {
                            has_mouse_event = true;
                            break;
                        }
                    }

                    if (!has_mouse_event) {
                        Ref<InputEventMouseMotion> mm(make_ref_counted<InputEventMouseMotion>());
                        mm->set_device(InputEvent::DEVICE_ID_INTERNAL);
                        mm->set_global_position(physics_last_mousepos);
                        mm->set_position(physics_last_mousepos);
                        mm->set_alt(physics_last_mouse_state.alt);
                        mm->set_shift(physics_last_mouse_state.shift);
                        mm->set_control(physics_last_mouse_state.control);
                        mm->set_metakey(physics_last_mouse_state.meta);
                        mm->set_button_mask(physics_last_mouse_state.mouse_mask);
                        physics_picking_events.push_back(mm);
                    }
                }

                while (!physics_picking_events.empty()) {

                    Ref<InputEvent> ev(eastl::move(physics_picking_events.front()));
                    physics_picking_events.pop_front();

                    Vector2 pos;
                    bool is_mouse = false;

                    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(ev);

                    if (mm) {

                        pos = mm->get_position();
                        is_mouse = true;

                        physics_has_last_mousepos = true;
                        physics_last_mousepos = pos;
                        physics_last_mouse_state.alt = mm->get_alt();
                        physics_last_mouse_state.shift = mm->get_shift();
                        physics_last_mouse_state.control = mm->get_control();
                        physics_last_mouse_state.meta = mm->get_metakey();
                        physics_last_mouse_state.mouse_mask = mm->get_button_mask();
                    }

                    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(ev);

                    if (mb) {

                        pos = mb->get_position();
                        is_mouse = true;

                        physics_has_last_mousepos = true;
                        physics_last_mousepos = pos;
                        physics_last_mouse_state.alt = mb->get_alt();
                        physics_last_mouse_state.shift = mb->get_shift();
                        physics_last_mouse_state.control = mb->get_control();
                        physics_last_mouse_state.meta = mb->get_metakey();

                        if (mb->is_pressed()) {
                            physics_last_mouse_state.mouse_mask |= (1 << (mb->get_button_index() - 1));
                        } else {
                            physics_last_mouse_state.mouse_mask &= ~(1 << (mb->get_button_index() - 1));

                            // If touch mouse raised, assume we don't know last mouse pos until new events come
                            if (mb->get_device() == InputEvent::DEVICE_ID_TOUCH_MOUSE) {
                                physics_has_last_mousepos = false;
                            }
                        }
                    }

                    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(ev);
                    if (k) {
                        //only for mask
                        physics_last_mouse_state.alt = k->get_alt();
                        physics_last_mouse_state.shift = k->get_shift();
                        physics_last_mouse_state.control = k->get_control();
                        physics_last_mouse_state.meta = k->get_metakey();
                        continue;
                    }

                    Ref<InputEventScreenDrag> sd = dynamic_ref_cast<InputEventScreenDrag>(ev);

                    if (sd) {
                        pos = sd->get_position();
                    }

                    Ref<InputEventScreenTouch> st = dynamic_ref_cast<InputEventScreenTouch>(ev);

                    if (st) {
                        pos = st->get_position();
                    }

                    if (ss2d) {
                        //send to 2D

                        uint64_t frame = get_tree()->get_frame();

                        PhysicsDirectSpaceState2D::ShapeResult res[64];
                        for (CanvasLayer * E : canvas_layers) {
                            Transform2D canvas_transform;
                            ObjectID canvas_layer_id;
                            if (E) {
                                // A descendant CanvasLayer
                                canvas_transform = E->get_transform();
                                canvas_layer_id = E->get_instance_id();
                            } else {
                                // This Viewport's builtin canvas
                                canvas_transform = get_canvas_transform();
                                canvas_layer_id = 0;
                            }

                            Vector2 point = canvas_transform.affine_inverse().xform(pos);

                            int rc = ss2d->intersect_point_on_canvas(point, canvas_layer_id, res, 64, HashSet<RID>(), 0xFFFFFFFF, true, true, true);
                            for (int i = 0; i < rc; i++) {

                                if (res[i].collider_id.is_valid() && res[i].collider) {
                                    CollisionObject2D *co = object_cast<CollisionObject2D>(res[i].collider);
                                    if (co) {
                                        bool send_event = true;
                                        if (is_mouse) {
                                            HashMap<ObjectID, uint64_t>::iterator F = physics_2d_mouseover.find(res[i].collider_id);

                                            if (F==physics_2d_mouseover.end()) {
                                                physics_2d_mouseover.emplace(res[i].collider_id, frame);
                                                co->_mouse_enter();
                                            } else {
                                                F->second = frame;
                                                // It was already hovered, so don't send the event if it's faked
                                                if (mm && mm->get_device() == InputEvent::DEVICE_ID_INTERNAL) {
                                                    send_event = false;
                                                }
                                            }
                                        }

                                        if (send_event) {
                                            co->_input_event(this, ev, res[i].shape);
                                        }
                                    }
                                }
                            }
                        }

                        if (is_mouse) {
                            for (HashMap<const ObjectID,uint64_t>::iterator iter =physics_2d_mouseover.begin(); iter!=physics_2d_mouseover.end(); ) {
                                if (iter->second == frame) {
                                    ++iter;
                                    continue;
                                }
                                Object *o = gObjectDB().get_instance(iter->first);
                                iter = physics_2d_mouseover.erase(iter);
                                if (o) {

                                    CollisionObject2D *co = object_cast<CollisionObject2D>(o);
                                    if (co) {
                                        co->_mouse_exit();
                                    }
                                }
                            }
                        }
                    }

#ifndef _3D_DISABLED
                    bool captured = false;

                    if (physics_object_capture.is_valid()) {

                        CollisionObject3D *co = object_cast<CollisionObject3D>(gObjectDB().get_instance(physics_object_capture));
                        if (co && camera) {
                            _collision_object_input_event(co, camera, ev, Vector3(), Vector3(), 0);
                            captured = true;
                            if (mb && mb->get_button_index() == 1 && !mb->is_pressed()) {
                                physics_object_capture = 0;
                            }

                        } else {
                            physics_object_capture = 0;
                        }
                    }

                    if (captured) {
                        //none
                    } else if (pos == last_pos) {

                        if (last_id.is_valid()) {
                            if (gObjectDB().get_instance(last_id) && last_object) {
                                //good, exists
                                _collision_object_input_event(last_object, camera, ev, result.position, result.normal, result.shape);
                                if (last_object->get_capture_input_on_drag() && mb && mb->get_button_index() == 1 && mb->is_pressed()) {
                                    physics_object_capture = last_id;
                                }
                            }
                        }
                    } else {

                        if (camera) {

                            Vector3 from = camera->project_ray_origin(pos);
                            Vector3 dir = camera->project_ray_normal(pos);

                            PhysicsDirectSpaceState3D *space = PhysicsServer3D::get_singleton()->space_get_direct_state(find_world()->get_space());
                            if (space) {

                                bool col = space->intersect_ray(from, from + dir * 10000, result, HashSet<RID>(), 0xFFFFFFFF, true, true, true);
                                ObjectID new_collider = ObjectID(0ULL);
                                if (col) {

                                    CollisionObject3D *co = object_cast<CollisionObject3D>(result.collider);
                                    if (co) {

                                        _collision_object_input_event(co, camera, ev, result.position, result.normal, result.shape);
                                        last_object = co;
                                        last_id = result.collider_id;
                                        new_collider = last_id;
                                        if (co->get_capture_input_on_drag() && mb && mb->get_button_index() == 1 && mb->is_pressed()) {
                                            physics_object_capture = last_id;
                                        }
                                    }
                                }

                                if (is_mouse && new_collider != physics_object_over) {

                                    if (physics_object_over.is_valid()) {

                                        CollisionObject3D *co = object_cast<CollisionObject3D>(gObjectDB().get_instance(physics_object_over));
                                        if (co) {
                                            co->_mouse_exit();
                                        }
                                    }

                                    if (new_collider.is_valid()) {

                                        CollisionObject3D *co = object_cast<CollisionObject3D>(gObjectDB().get_instance(new_collider));
                                        if (co) {
                                            co->_mouse_enter();
                                        }
                                    }

                                    physics_object_over = new_collider;
                                }
                            }

                            last_pos = pos;
                        }
                    }
#endif
                }
            }

        } break;
        case SceneTree::NOTIFICATION_WM_MOUSE_EXIT:
        case SceneTree::NOTIFICATION_WM_FOCUS_OUT: {

            _drop_physics_mouseover();

            if (gui.mouse_focus) {
                //if mouse is being pressed, send a release event
                _drop_mouse_focus();
            }
        } break;
    }
}

RID Viewport::get_viewport_rid() const {

    return viewport;
}

void Viewport::set_use_arvr(bool p_use_arvr) {
    arvr = p_use_arvr;

    RenderingServer::get_singleton()->viewport_set_use_arvr(viewport, arvr);
}

bool Viewport::use_arvr() {
    return arvr;
}

void Viewport::update_canvas_items() {
    if (!is_inside_tree())
        return;

    _update_canvas_items(this);
}

void Viewport::set_size(const Size2 &p_size) {

    if (size == p_size.floor())
        return;
    size = p_size.floor();
    RenderingServer::get_singleton()->viewport_set_size(viewport, size.width, size.height);

    _update_stretch_transform();

    emit_signal("size_changed");
}

Rect2 Viewport::get_visible_rect() const {

    Rect2 r;

    if (size == Size2()) {
        r = Rect2(Point2(), OS::get_singleton()->get_window_size());
    } else {
        r = Rect2(Point2(), size);
    }

    if (size_override) {
        r.size = size_override_size;
    }

    return r;
}

Size2 Viewport::get_size() const {

    return size;
}

void Viewport::_update_listener() {
    /*
    if (is_inside_tree() && audio_listener && (camera || listener) && (!get_parent() || (object_cast<Control>(get_parent()) && object_cast<Control>(get_parent())->is_visible_in_tree())))  {
        SpatialSoundServer::get_singleton()->listener_set_space(internal_listener, find_world()->get_sound_space());
    } else {
        SpatialSoundServer::get_singleton()->listener_set_space(internal_listener, RID());
    }
*/
}

void Viewport::_update_listener_2d() {

    /*
    if (is_inside_tree() && audio_listener && (!get_parent() || (object_cast<Control>(get_parent()) && object_cast<Control>(get_parent())->is_visible_in_tree())))
        SpatialSound2DServer::get_singleton()->listener_set_space(internal_listener_2d, find_world_2d()->get_sound_space());
    else
        SpatialSound2DServer::get_singleton()->listener_set_space(internal_listener_2d, RID());
*/
}

void Viewport::set_as_audio_listener(bool p_enable) {

    if (p_enable == audio_listener)
        return;

    audio_listener = p_enable;
    _update_listener();
}

bool Viewport::is_audio_listener() const {

    return audio_listener;
}

void Viewport::set_as_audio_listener_2d(bool p_enable) {

    if (p_enable == audio_listener_2d)
        return;

    audio_listener_2d = p_enable;

    _update_listener_2d();
}

bool Viewport::is_audio_listener_2d() const {

    return audio_listener_2d;
}

void Viewport::enable_canvas_transform_override(bool p_enable) {
    if (override_canvas_transform == p_enable) {
        return;
    }

    override_canvas_transform = p_enable;
    if (p_enable) {
        RenderingServer::get_singleton()->viewport_set_canvas_transform(viewport, find_world_2d()->get_canvas(), canvas_transform_override);
    } else {
        RenderingServer::get_singleton()->viewport_set_canvas_transform(viewport, find_world_2d()->get_canvas(), canvas_transform);
    }
}

bool Viewport::is_canvas_transform_override_enbled() const {
    return override_canvas_transform;
}

void Viewport::set_canvas_transform_override(const Transform2D &p_transform) {
    if (canvas_transform_override == p_transform) {
        return;
    }

    canvas_transform_override = p_transform;
    if (override_canvas_transform) {
        RenderingServer::get_singleton()->viewport_set_canvas_transform(viewport, find_world_2d()->get_canvas(), canvas_transform_override);
    }
}

Transform2D Viewport::get_canvas_transform_override() const {
    return canvas_transform_override;
}

void Viewport::set_canvas_transform(const Transform2D &p_transform) {

    canvas_transform = p_transform;
    if (!override_canvas_transform) {
        RenderingServer::get_singleton()->viewport_set_canvas_transform(viewport, find_world_2d()->get_canvas(), canvas_transform);
    }
}

Transform2D Viewport::get_canvas_transform() const {

    return canvas_transform;
}

void Viewport::_update_global_transform() {

    Transform2D sxform = stretch_transform * global_canvas_transform;

    RenderingServer::get_singleton()->viewport_set_global_canvas_transform(viewport, sxform);
}

void Viewport::set_global_canvas_transform(const Transform2D &p_transform) {

    global_canvas_transform = p_transform;

    _update_global_transform();
}

Transform2D Viewport::get_global_canvas_transform() const {

    return global_canvas_transform;
}

void Viewport::_listener_transform_changed_notify() {

#ifndef _3D_DISABLED
//if (listener)
//		SpatialSoundServer::get_singleton()->listener_set_transform(internal_listener, listener->get_listener_transform());
#endif
}

void Viewport::_listener_set(Listener3D *p_listener) {

#ifndef _3D_DISABLED

    if (listener == p_listener)
        return;

    listener = p_listener;

    _update_listener();
    _listener_transform_changed_notify();
#endif
}

bool Viewport::_listener_add(Listener3D *p_listener) {

    listeners.insert(p_listener);
    return listeners.size() == 1;
}

void Viewport::_listener_remove(Listener3D *p_listener) {

    listeners.erase(p_listener);
    if (listener == p_listener) {
        listener = nullptr;
    }
}

#ifndef _3D_DISABLED
void Viewport::_listener_make_next_current(Listener3D *p_exclude) {

    if (!listeners.empty()) {
        for (Listener3D * E : listeners) {

            if (p_exclude == E)
                continue;
            if (!E->is_inside_tree())
                continue;
            if (listener != nullptr)
                return;

            E->make_current();
        }
    } else {
        // Attempt to reset listener to the camera position
        if (camera != nullptr) {
            _update_listener();
            _camera_transform_changed_notify();
        }
    }
}
#endif

void Viewport::_camera_transform_changed_notify() {

#ifndef _3D_DISABLED
// If there is an active listener in the scene, it takes priority over the camera
//	if (camera && !listener)
//		SpatialSoundServer::get_singleton()->listener_set_transform(internal_listener, camera->get_camera_transform());
#endif
}

void Viewport::_camera_set(Camera3D *p_camera) {

#ifndef _3D_DISABLED

    if (camera == p_camera)
        return;

    if (camera) {
        camera->notification(Camera3D::NOTIFICATION_LOST_CURRENT);
    }
    camera = p_camera;
    if (!camera_override) {
        if (camera)
            RenderingServer::get_singleton()->viewport_attach_camera(viewport, camera->get_camera_rid());
        else
            RenderingServer::get_singleton()->viewport_attach_camera(viewport, RID());
    }
    if (camera) {
        camera->notification(Camera3D::NOTIFICATION_BECAME_CURRENT);
    }

    _update_listener();
    _camera_transform_changed_notify();
#endif
}

bool Viewport::_camera_add(Camera3D *p_camera) {

    cameras.insert(p_camera);
    return cameras.size() == 1;
}

void Viewport::_camera_remove(Camera3D *p_camera) {

    cameras.erase(p_camera);
    if (camera == p_camera) {
        camera->notification(Camera3D::NOTIFICATION_LOST_CURRENT);
        camera = nullptr;
    }
}

#ifndef _3D_DISABLED
void Viewport::_camera_make_next_current(Camera3D *p_exclude) {

    for (Camera3D * E : cameras) {

        if (p_exclude == E)
            continue;
        if (!E->is_inside_tree())
            continue;
        if (camera != nullptr)
            return;

        E->make_current();
    }
}
#endif

void Viewport::_canvas_layer_add(CanvasLayer *p_canvas_layer) {

    canvas_layers.insert(p_canvas_layer);
}

void Viewport::_canvas_layer_remove(CanvasLayer *p_canvas_layer) {

    canvas_layers.erase(p_canvas_layer);
}

void Viewport::set_transparent_background(bool p_enable) {

    transparent_bg = p_enable;
    RenderingServer::get_singleton()->viewport_set_transparent_background(viewport, p_enable);
}

bool Viewport::has_transparent_background() const {

    return transparent_bg;
}

void Viewport::set_world_2d(const Ref<World2D> &p_world_2d) {
    if (world_2d == p_world_2d)
        return;

    if (parent && parent->find_world_2d() == p_world_2d) {
        WARN_PRINT("Unable to use parent world as world_2d");
        return;
    }

    if (is_inside_tree()) {
        find_world_2d()->_remove_viewport(this);
        RenderingServer::get_singleton()->viewport_remove_canvas(viewport, current_canvas);
    }

    if (p_world_2d)
        world_2d = p_world_2d;
    else {
        WARN_PRINT("Invalid world");
        world_2d = make_ref_counted<World2D>();
    }

    _update_listener_2d();

    if (is_inside_tree()) {
        current_canvas = find_world_2d()->get_canvas();
        RenderingServer::get_singleton()->viewport_attach_canvas(viewport, current_canvas);
        find_world_2d()->_register_viewport(this, Rect2());
    }
}

Ref<World2D> Viewport::find_world_2d() const {

    if (world_2d)
        return world_2d;
    else if (parent)
        return parent->find_world_2d();
    else
        return Ref<World2D>();
}

void Viewport::_propagate_enter_world(Node *p_node) {

    if (p_node != this) {

        if (!p_node->is_inside_tree()) //may not have entered scene yet
            return;

        if (object_cast<Node3D>(p_node) || object_cast<WorldEnvironment>(p_node)) {

            p_node->notification(Node3D::NOTIFICATION_ENTER_WORLD);
        } else {
            Viewport *v = object_cast<Viewport>(p_node);
            if (v) {

                if (v->world || v->own_world)
                    return;
            }
        }
    }

    for (int i = 0; i < p_node->get_child_count(); i++) {

        _propagate_enter_world(p_node->get_child(i));
    }
}

void Viewport::_propagate_viewport_notification(Node *p_node, int p_what) {

    p_node->notification(p_what);
    for (int i = 0; i < p_node->get_child_count(); i++) {
        Node *c = p_node->get_child(i);
        if (object_cast<Viewport>(c))
            continue;
        _propagate_viewport_notification(c, p_what);
    }
}

void Viewport::_propagate_exit_world(Node *p_node) {

    if (p_node != this) {

        if (!p_node->is_inside_tree()) //may have exited scene already
            return;

        if (object_cast<Node3D>(p_node) || object_cast<WorldEnvironment>(p_node)) {

            p_node->notification(Node3D::NOTIFICATION_EXIT_WORLD);
        } else {
            Viewport *v = object_cast<Viewport>(p_node);
            if (v) {

                if (v->world || v->own_world)
                    return;
            }
        }
    }

    for (int i = 0; i < p_node->get_child_count(); i++) {

        _propagate_exit_world(p_node->get_child(i));
    }
}

void Viewport::set_world(const Ref<World3D> &p_world) {

    if (world == p_world)
        return;

    if (is_inside_tree())
        _propagate_exit_world(this);

    if (own_world && world) {
        world->disconnect(CoreStringNames::get_singleton()->changed, this, "_own_world_changed");
    }
    world = p_world;
    if (own_world) {
        if (world) {
            own_world = dynamic_ref_cast<World3D>(world->duplicate());
            world->connect(CoreStringNames::get_singleton()->changed, this, "_own_world_changed");
        } else {
            own_world = Ref<World3D>(memnew(World3D));
        }
    }

    if (is_inside_tree())
        _propagate_enter_world(this);

    if (is_inside_tree()) {
        RenderingServer::get_singleton()->viewport_set_scenario(viewport, find_world()->get_scenario());
    }

    _update_listener();
}

Ref<World3D> Viewport::get_world() const {

    return world;
}

Ref<World2D> Viewport::get_world_2d() const {

    return world_2d;
}

Ref<World3D> Viewport::find_world() const {

    if (own_world)
        return own_world;
    else if (world)
        return world;
    else if (parent)
        return parent->find_world();
    else
        return Ref<World3D>();
}

Listener3D *Viewport::get_listener() const {

    return listener;
}

Camera3D *Viewport::get_camera() const {

    return camera;
}

void Viewport::enable_camera_override(bool p_enable) {

#ifndef _3D_DISABLED
    if (p_enable == camera_override) {
        return;
    }

    if (p_enable) {
        camera_override.rid = RenderingServer::get_singleton()->camera_create();
    } else {
        RenderingServer::get_singleton()->free_rid(camera_override.rid);
        camera_override.rid = RID();
    }

    if (p_enable) {
        RenderingServer::get_singleton()->viewport_attach_camera(viewport, camera_override.rid);
    } else if (camera) {
        RenderingServer::get_singleton()->viewport_attach_camera(viewport, camera->get_camera_rid());
    } else {
        RenderingServer::get_singleton()->viewport_attach_camera(viewport, RID());
    }
#endif
}

bool Viewport::is_camera_override_enabled() const {
    return camera_override;
}

void Viewport::set_camera_override_transform(const Transform &p_transform) {
    if (camera_override) {
        camera_override.transform = p_transform;
        RenderingServer::get_singleton()->camera_set_transform(camera_override.rid, p_transform);
    }
}

Transform Viewport::get_camera_override_transform() const {
    if (camera_override) {
        return camera_override.transform;
    }

    return Transform();
}

void Viewport::set_camera_override_perspective(float p_fovy_degrees, float p_z_near, float p_z_far) {
    if (camera_override) {
        if (camera_override.fov == p_fovy_degrees && camera_override.z_near == p_z_near &&
                camera_override.z_far == p_z_far && camera_override.projection == CameraOverrideData::PROJECTION_PERSPECTIVE)
            return;

        camera_override.fov = p_fovy_degrees;
        camera_override.z_near = p_z_near;
        camera_override.z_far = p_z_far;
        camera_override.projection = CameraOverrideData::PROJECTION_PERSPECTIVE;

        RenderingServer::get_singleton()->camera_set_perspective(camera_override.rid, camera_override.fov, camera_override.z_near, camera_override.z_far);
    }
}

void Viewport::set_camera_override_orthogonal(float p_size, float p_z_near, float p_z_far) {
    if (camera_override) {
        if (camera_override.size == p_size && camera_override.z_near == p_z_near &&
                camera_override.z_far == p_z_far && camera_override.projection == CameraOverrideData::PROJECTION_ORTHOGONAL)
            return;

        camera_override.size = p_size;
        camera_override.z_near = p_z_near;
        camera_override.z_far = p_z_far;
        camera_override.projection = CameraOverrideData::PROJECTION_ORTHOGONAL;

        RenderingServer::get_singleton()->camera_set_orthogonal(camera_override.rid, camera_override.size, camera_override.z_near, camera_override.z_far);
    }
}

Transform2D Viewport::get_final_transform() const {

    return stretch_transform * global_canvas_transform;
}

void Viewport::_update_canvas_items(Node *p_node) {
    if (p_node != this) {

        Viewport *vp = object_cast<Viewport>(p_node);
        if (vp)
            return;

        CanvasItem *ci = object_cast<CanvasItem>(p_node);
        if (ci) {
            ci->update();
        }
    }

    int cc = p_node->get_child_count();

    for (int i = 0; i < cc; i++) {
        _update_canvas_items(p_node->get_child(i));
    }
}

void Viewport::set_size_override(bool p_enable, const Size2 &p_size, const Vector2 &p_margin) {

    if (size_override == p_enable && p_size == size_override_size)
        return;

    size_override = p_enable;
    if (p_size.x >= 0 || p_size.y >= 0) {
        size_override_size = p_size;
    }
    size_override_margin = p_margin;

    _update_stretch_transform();
    emit_signal("size_changed");
}

Size2 Viewport::get_size_override() const {

    return size_override_size;
}
bool Viewport::is_size_override_enabled() const {

    return size_override;
}
void Viewport::set_size_override_stretch(bool p_enable) {

    if (p_enable == size_override_stretch)
        return;

    size_override_stretch = p_enable;

    _update_stretch_transform();
}

bool Viewport::is_size_override_stretch_enabled() const {

    return size_override_stretch;
}

void Viewport::set_update_mode(UpdateMode p_mode) {

    update_mode = p_mode;
    RenderingServer::get_singleton()->viewport_set_update_mode(viewport, RS::ViewportUpdateMode(p_mode));
}
Viewport::UpdateMode Viewport::get_update_mode() const {

    return update_mode;
}

Ref<ViewportTexture> Viewport::get_texture() const {

    return default_texture;
}

void Viewport::set_vflip(bool p_enable) {

    vflip = p_enable;
    RenderingServer::get_singleton()->viewport_set_vflip(viewport, p_enable);
}

bool Viewport::get_vflip() const {

    return vflip;
}

void Viewport::set_clear_mode(ClearMode p_mode) {

    clear_mode = p_mode;
    RenderingServer::get_singleton()->viewport_set_clear_mode(viewport, RS::ViewportClearMode(p_mode));
}

Viewport::ClearMode Viewport::get_clear_mode() const {

    return clear_mode;
}

void Viewport::set_shadow_atlas_size(int p_size) {

    if (shadow_atlas_size == p_size)
        return;

    shadow_atlas_size = p_size;
    RenderingServer::get_singleton()->viewport_set_shadow_atlas_size(viewport, p_size);
}

int Viewport::get_shadow_atlas_size() const {

    return shadow_atlas_size;
}

void Viewport::set_shadow_atlas_quadrant_subdiv(int p_quadrant, ShadowAtlasQuadrantSubdiv p_subdiv) {

    ERR_FAIL_INDEX(p_quadrant, 4);
    ERR_FAIL_INDEX(p_subdiv, SHADOW_ATLAS_QUADRANT_SUBDIV_MAX);

    if (shadow_atlas_quadrant_subdiv[p_quadrant] == p_subdiv)
        return;

    shadow_atlas_quadrant_subdiv[p_quadrant] = p_subdiv;
    static const int subdiv[SHADOW_ATLAS_QUADRANT_SUBDIV_MAX] = { 0, 1, 4, 16, 64, 256, 1024 };

    RenderingServer::get_singleton()->viewport_set_shadow_atlas_quadrant_subdivision(viewport, p_quadrant, subdiv[p_subdiv]);
}
Viewport::ShadowAtlasQuadrantSubdiv Viewport::get_shadow_atlas_quadrant_subdiv(int p_quadrant) const {

    ERR_FAIL_INDEX_V(p_quadrant, 4, SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
    return shadow_atlas_quadrant_subdiv[p_quadrant];
}

Transform2D Viewport::_get_input_pre_xform() const {

    Transform2D pre_xf;

    if (to_screen_rect != Rect2()) {

        pre_xf.elements[2] = -to_screen_rect.position;
        pre_xf.scale(size / to_screen_rect.size);
    }

    return pre_xf;
}

Vector2 Viewport::_get_window_offset() const {

    if (get_parent() && get_parent()->has_method("get_global_position")) {
        return get_parent()->call_va("get_global_position");
    }
    return Vector2();
}

Ref<InputEvent> Viewport::_make_input_local(const Ref<InputEvent> &ev) {

    Vector2 vp_ofs = _get_window_offset();
    Transform2D ai = get_final_transform().affine_inverse() * _get_input_pre_xform();

    return ev->xformed_by(ai, -vp_ofs);
}

void Viewport::_vp_input_text(StringView p_text) {

    if (gui.key_focus) {
        gui.key_focus->call_va("set_text", p_text);
    }
}

void Viewport::_vp_input(const Ref<InputEvent> &p_ev) {

    if (disable_input)
        return;

#ifdef TOOLS_ENABLED
    if (Engine::get_singleton()->is_editor_hint() && get_tree()->get_edited_scene_root() && get_tree()->get_edited_scene_root()->is_a_parent_of(this)) {
        return;
    }
#endif

    if (to_screen_rect == Rect2())
        return; //if render target, can't get input events

    //this one handles system input, p_ev are in system coordinates
    //they are converted to viewport coordinates

    Ref<InputEvent> ev = _make_input_local(p_ev);
    input(ev);
}

void Viewport::_vp_unhandled_input(const Ref<InputEvent> &p_ev) {

    if (disable_input)
        return;
#ifdef TOOLS_ENABLED
    if (Engine::get_singleton()->is_editor_hint() && get_tree()->get_edited_scene_root() && get_tree()->get_edited_scene_root()->is_a_parent_of(this)) {
        return;
    }
#endif

    /*
    if (parent_control && !parent_control->is_visible_in_tree())
        return;
    */

    if (to_screen_rect == Rect2())
        return; //if render target, can't get input events

    //this one handles system input, p_ev are in system coordinates
    //they are converted to viewport coordinates

    Ref<InputEvent> ev = _make_input_local(p_ev);
    unhandled_input(ev);
}

Vector2 Viewport::get_mouse_position() const {

    return (get_final_transform().affine_inverse() * _get_input_pre_xform()).xform(Input::get_singleton()->get_mouse_position() - _get_window_offset());
}

void Viewport::warp_mouse(const Vector2 &p_pos) {

    Vector2 gpos = (get_final_transform().affine_inverse() * _get_input_pre_xform()).affine_inverse().xform(p_pos);
    Input::get_singleton()->warp_mouse_position(gpos);
}

void Viewport::_gui_prepare_subwindows() {

    if (gui.subwindow_visibility_dirty) {

        gui.subwindows.clear();
        for (Control * E : gui.all_known_subwindows) {
            if (E->is_visible_in_tree()) {
                gui.subwindows.push_back(E);
            }
        }

        gui.subwindow_visibility_dirty = false;
        gui.subwindow_order_dirty = true;
    }

    _gui_sort_subwindows();
}

void Viewport::_gui_sort_subwindows() {

    if (!gui.subwindow_order_dirty)
        return;

    eastl::sort(gui.modal_stack.begin(), gui.modal_stack.end(),Control::CComparator());
    eastl::sort(gui.subwindows.begin(), gui.subwindows.end(), Control::CComparator());

    gui.subwindow_order_dirty = false;
}

void Viewport::_gui_sort_modal_stack() {

    eastl::sort(gui.modal_stack.begin(), gui.modal_stack.end(),Control::CComparator());
}

void Viewport::_gui_sort_roots() {

    if (!gui.roots_order_dirty)
        return;

    eastl::sort(gui.roots.begin(), gui.roots.end(), Control::CComparator());

    gui.roots_order_dirty = false;
}

void Viewport::_gui_cancel_tooltip() {

    gui.tooltip = nullptr;
    gui.tooltip_timer = -1;
    if (gui.tooltip_popup) {
        gui.tooltip_popup->queue_delete();
        gui.tooltip_popup = nullptr;
        gui.tooltip_label = nullptr;
    }
}

StringName Viewport::_gui_get_tooltip(Control *p_control, const Vector2 &p_pos, Control **r_which) {

    Vector2 pos = p_pos;
    StringName tooltip;

    while (p_control) {

        tooltip = p_control->get_tooltip(pos);

        if (r_which) {
            *r_which = p_control;
        }

        if (!tooltip.empty())
            break;
        pos = p_control->get_transform().xform(pos);

        if (p_control->data.mouse_filter == Control::MOUSE_FILTER_STOP)
            break;
        if (p_control->is_set_as_toplevel())
            break;

        p_control = p_control->get_parent_control();
    }

    return tooltip;
}

void Viewport::_gui_show_tooltip() {

    if (!gui.tooltip) {
        return;
    }

    Control *which = nullptr;
    StringName tooltip = _gui_get_tooltip(gui.tooltip, gui.tooltip->get_global_transform().xform_inv(gui.tooltip_pos), &which);
    tooltip =StringName(StringUtils::strip_edges( tooltip));
    if (tooltip.empty())
        return; // bye

    if (gui.tooltip_popup) {
        memdelete(gui.tooltip_popup);
        gui.tooltip_popup = nullptr;
        gui.tooltip_label = nullptr;
    }

    if (!which) {
        return;
    }

    Control *rp = which;

    gui.tooltip_popup = which->make_custom_tooltip(tooltip);

    if (!gui.tooltip_popup) {
        gui.tooltip_popup = memnew(TooltipPanel);

        gui.tooltip_label = memnew(TooltipLabel);
        gui.tooltip_popup->add_child(gui.tooltip_label);

        Ref<StyleBox> ttp = gui.tooltip_label->get_stylebox("panel", "TooltipPanel");

        gui.tooltip_label->set_anchor_and_margin(Margin::Left, Control::ANCHOR_BEGIN, ttp->get_margin(Margin::Left));
        gui.tooltip_label->set_anchor_and_margin(Margin::Top, Control::ANCHOR_BEGIN, ttp->get_margin(Margin::Top));
        gui.tooltip_label->set_anchor_and_margin(Margin::Right, Control::ANCHOR_END, -ttp->get_margin(Margin::Right));
        gui.tooltip_label->set_anchor_and_margin(Margin::Bottom, Control::ANCHOR_END, -ttp->get_margin(Margin::Bottom));
        gui.tooltip_label->set_text(tooltip);
    }

    rp->add_child(gui.tooltip_popup);
    gui.tooltip_popup->force_parent_owned();
    gui.tooltip_popup->set_as_toplevel(true);
    if (gui.tooltip) // Avoids crash when rapidly switching controls.
        gui.tooltip_popup->set_scale(gui.tooltip->get_global_transform().get_scale());

    Point2 tooltip_offset = ProjectSettings::get_singleton()->get("display/mouse_cursor/tooltip_position_offset");
    Rect2 r(gui.tooltip_pos + tooltip_offset, gui.tooltip_popup->get_minimum_size());
    Rect2 vr = gui.tooltip_popup->get_viewport_rect();
    if (r.size.x * gui.tooltip_popup->get_scale().x + r.position.x > vr.size.x)
        r.position.x = vr.size.x - r.size.x * gui.tooltip_popup->get_scale().x;
    else if (r.position.x < 0)
        r.position.x = 0;
    r.position.y = CLAMP(r.position.y,0,vr.size.y - r.size.y * gui.tooltip_popup->get_scale().y);

    gui.tooltip_popup->set_global_position(r.position);
    gui.tooltip_popup->set_size(r.size);

    gui.tooltip_popup->raise();
    gui.tooltip_popup->show();
}

void Viewport::_gui_call_input(Control *p_control, const Ref<InputEvent> &p_input) {

    //_block();

    Ref<InputEvent> ev = p_input;

    //mouse wheel events can't be stopped
    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_input);

    bool cant_stop_me_now = (mb &&
                             (mb->get_button_index() == BUTTON_WHEEL_DOWN ||
                                     mb->get_button_index() == BUTTON_WHEEL_UP ||
                                     mb->get_button_index() == BUTTON_WHEEL_LEFT ||
                                     mb->get_button_index() == BUTTON_WHEEL_RIGHT));
    Ref<InputEventPanGesture> pn = dynamic_ref_cast<InputEventPanGesture>(p_input);
    cant_stop_me_now = pn || cant_stop_me_now;

    bool ismouse = ev || object_cast<InputEventMouseMotion>(p_input.get()) != nullptr;

    CanvasItem *ci = p_control;
    while (ci) {

        Control *control = object_cast<Control>(ci);
        if (control) {

            if (control->data.mouse_filter != Control::MOUSE_FILTER_IGNORE) {
                control->emit_signal(SceneStringNames::get_singleton()->gui_input, ev); //signal should be first, so it's possible to override an event (and then accept it)
            }
            if (gui.key_event_accepted)
                break;
            if (!control->is_inside_tree())
                break;

            if (control->data.mouse_filter != Control::MOUSE_FILTER_IGNORE) {
                control->call_multilevel(SceneStringNames::get_singleton()->_gui_input, ev);
            }

            if (!control->is_inside_tree() || control->is_set_as_toplevel())
                break;
            if (gui.key_event_accepted)
                break;
            if (!cant_stop_me_now && control->data.mouse_filter == Control::MOUSE_FILTER_STOP && ismouse)
                break;
        }

        if (ci->is_set_as_toplevel())
            break;

        ev = ev->xformed_by(ci->get_transform()); //transform event upwards
        ci = ci->get_parent_item();
    }

    //_unblock();
}

void Viewport::_gui_call_notification(Control *p_control, int p_what) {

    CanvasItem *ci = p_control;
    while (ci) {

        Control *control = object_cast<Control>(ci);
        if (control) {

            if (control->data.mouse_filter != Control::MOUSE_FILTER_IGNORE) {
                control->notification(p_what);
            }

            if (!control->is_inside_tree())
                break;

            if (!control->is_inside_tree() || control->is_set_as_toplevel())
                break;
            if (control->data.mouse_filter == Control::MOUSE_FILTER_STOP)
                break;
        }

        if (ci->is_set_as_toplevel())
            break;

        ci = ci->get_parent_item();
    }

    //_unblock();
}
Control *Viewport::_gui_find_control(const Point2 &p_global) {

    _gui_prepare_subwindows();

    for (auto riter = gui.subwindows.rbegin(); riter!= gui.subwindows.rend(); ++riter) {

        Control *sw = *riter;
        if (!sw->is_visible_in_tree())
            continue;

        Transform2D xform;
        CanvasItem *pci = sw->get_parent_item();
        if (pci)
            xform = pci->get_global_transform_with_canvas();
        else
            xform = sw->get_canvas_transform();

        Control *ret = _gui_find_control_at_pos(sw, p_global, xform, gui.focus_inv_xform);
        if (ret)
            return ret;
    }

    _gui_sort_roots();

    for (auto riter = gui.roots.rbegin(); riter != gui.roots.rend(); ++riter) {

        Control *sw = *riter;
        if (!sw->is_visible_in_tree())
            continue;

        Transform2D xform;
        CanvasItem *pci = sw->get_parent_item();
        if (pci)
            xform = pci->get_global_transform_with_canvas();
        else
            xform = sw->get_canvas_transform();

        Control *ret = _gui_find_control_at_pos(sw, p_global, xform, gui.focus_inv_xform);
        if (ret)
            return ret;
    }

    return nullptr;
}

Control *Viewport::_gui_find_control_at_pos(CanvasItem *p_node, const Point2 &p_global, const Transform2D &p_xform, Transform2D &r_inv_xform) {

    if (object_cast<Viewport>(p_node))
        return nullptr;

    //subwindows first!!

    if (!p_node->is_visible()) {
        //return _find_next_visible_control_at_pos(p_node,p_global,r_inv_xform);
        return nullptr; //canvas item hidden, discard
    }

    Transform2D matrix = p_xform * p_node->get_transform();
    // matrix.basis_determinant() == 0.0f implies that node does not exist on scene
    if (matrix.basis_determinant() == 0.0f)
        return nullptr;

    Control *c = object_cast<Control>(p_node);

    if (!c || !c->clips_input() || c->has_point(matrix.affine_inverse().xform(p_global))) {

        for (int i = p_node->get_child_count() - 1; i >= 0; i--) {

            if (p_node == gui.tooltip_popup)
                continue;

            CanvasItem *ci = object_cast<CanvasItem>(p_node->get_child(i));
            if (!ci || ci->is_set_as_toplevel())
                continue;

            Control *ret = _gui_find_control_at_pos(ci, p_global, matrix, r_inv_xform);
            if (ret)
                return ret;
        }
    }

    if (!c)
        return nullptr;

    matrix.affine_invert();

    //conditions for considering this as a valid control for return
    if (c->data.mouse_filter != Control::MOUSE_FILTER_IGNORE && c->has_point(matrix.xform(p_global)) && (!gui.drag_preview || (c != gui.drag_preview && !gui.drag_preview->is_a_parent_of(c)))) {
        r_inv_xform = matrix;
        return c;
    } else
        return nullptr;
}

bool Viewport::_gui_drop(Control *p_at_control, Point2 p_at_pos, bool p_just_check) {

    { //attempt grab, try parent controls too
        CanvasItem *ci = p_at_control;
        while (ci) {

            Control *control = object_cast<Control>(ci);
            if (control) {

                if (control->can_drop_data(p_at_pos, gui.drag_data)) {
                    if (!p_just_check) {
                        control->drop_data(p_at_pos, gui.drag_data);
                    }

                    return true;
                }

                if (control->data.mouse_filter == Control::MOUSE_FILTER_STOP)
                    break;
            }

            p_at_pos = ci->get_transform().xform(p_at_pos);

            if (ci->is_set_as_toplevel())
                break;

            ci = ci->get_parent_item();
        }
    }

    return false;
}

void Viewport::_gui_input_event(Ref<InputEvent> p_event) {

    ERR_FAIL_COND(not p_event);

    //?
    /*
    if (!is_visible()) {
        return; //simple and plain
    }
    */

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);

    if (mb) {

        gui.key_event_accepted = false;

        Point2 mpos = mb->get_position();
        if (mb->is_pressed()) {

            Size2 pos = mpos;
            if (gui.mouse_focus_mask) {

                //do not steal mouse focus and stuff while a focus mask exists
                gui.mouse_focus_mask |= 1 << (mb->get_button_index() - 1); //add the button to the mask
            } else {

                bool is_handled = false;

                _gui_sort_modal_stack();
                while (!gui.modal_stack.empty()) {

                    Control *top = gui.modal_stack.back();
                    Vector2 pos2 = top->get_global_transform_with_canvas().affine_inverse().xform(mpos);
                    if (!top->has_point(pos2)) {

                        if (top->data.modal_exclusive || top->data.modal_frame == Engine::get_singleton()->get_frames_drawn()) {
                            //cancel event, sorry, modal exclusive EATS UP ALL
                            //alternative, you can't pop out a window the same frame it was made modal (fixes many issues)
                            set_input_as_handled();

                            return; // no one gets the event if exclusive NO ONE
                        }

                        if (mb->get_button_index() == BUTTON_WHEEL_UP || mb->get_button_index() == BUTTON_WHEEL_DOWN || mb->get_button_index() == BUTTON_WHEEL_LEFT || mb->get_button_index() == BUTTON_WHEEL_RIGHT) {
                            //cancel scroll wheel events, only clicks should trigger focus changes.
                            set_input_as_handled();
                            return;
                        }
                        top->notification(Control::NOTIFICATION_MODAL_CLOSE);
                        top->_modal_stack_remove();
                        top->hide();

                        if (!top->pass_on_modal_close_click()) {
                            is_handled = true;
                        }
                    } else {
                        break;
                    }
                }

                if (is_handled) {
                    set_input_as_handled();
                    return;
                }

                //Matrix32 parent_xform;

                /*
                if (data.parent_canvas_item)
                    parent_xform=data.parent_canvas_item->get_global_transform();
                */

                gui.mouse_focus = _gui_find_control(pos);
                gui.last_mouse_focus = gui.mouse_focus;

                if (!gui.mouse_focus) {
                    gui.mouse_focus_mask = 0;
                    return;
                }

                gui.mouse_focus_mask = 1 << (mb->get_button_index() - 1);

                if (mb->get_button_index() == BUTTON_LEFT) {
                    gui.drag_accum = Vector2();
                    gui.drag_attempted = false;
                }
            }

            mb = dynamic_ref_cast<InputEventMouseButton>(mb->xformed_by(Transform2D())); // make a copy of the event

            mb->set_global_position(pos);

            pos = gui.focus_inv_xform.xform(pos);

            mb->set_position(pos);

#ifdef DEBUG_ENABLED
            if (ScriptDebugger::get_singleton() && gui.mouse_focus) {

                Array arr;
                arr.push_back(gui.mouse_focus->get_path());
                arr.push_back(gui.mouse_focus->get_class());
                ScriptDebugger::get_singleton()->send_message(("click_ctrl"), arr);
            }
#endif

            if (mb->get_button_index() == BUTTON_LEFT) { //assign focus
                CanvasItem *ci = gui.mouse_focus;
                while (ci) {

                    Control *control = object_cast<Control>(ci);
                    if (control) {
                        if (control->get_focus_mode() != Control::FOCUS_NONE) {
                            if (control != gui.key_focus) {
                                control->grab_focus();
                            }
                            break;
                        }

                        if (control->data.mouse_filter == Control::MOUSE_FILTER_STOP)
                            break;
                    }

                    if (ci->is_set_as_toplevel())
                        break;

                    ci = ci->get_parent_item();
                }
            }

            if (gui.mouse_focus && gui.mouse_focus->can_process()) {
                _gui_call_input(gui.mouse_focus, mb);
            }

            set_input_as_handled();

            if (gui.drag_data.get_type() != VariantType::NIL && mb->get_button_index() == BUTTON_LEFT) {

                //alternate drop use (when using force_drag(), as proposed by #5342
                if (gui.mouse_focus) {
                    _gui_drop(gui.mouse_focus, pos, false);
                }

                gui.drag_data = Variant();
                gui.dragging = false;

                if (gui.drag_preview) {
                    memdelete(gui.drag_preview);
                    gui.drag_preview = nullptr;
                }
                _propagate_viewport_notification(this, NOTIFICATION_DRAG_END);
                //change mouse accordingly
            }

            _gui_cancel_tooltip();
            //gui.tooltip_popup->hide();

        } else {

            if (gui.drag_data.get_type() != VariantType::NIL && mb->get_button_index() == BUTTON_LEFT) {

                if (gui.mouse_over) {
                    Size2 pos = mpos;
                    pos = gui.focus_inv_xform.xform(pos);

                    _gui_drop(gui.mouse_over, pos, false);
                }

                if (gui.drag_preview && mb->get_button_index() == BUTTON_LEFT) {
                    memdelete(gui.drag_preview);
                    gui.drag_preview = nullptr;
                }

                gui.drag_data = Variant();
                gui.dragging = false;
                _propagate_viewport_notification(this, NOTIFICATION_DRAG_END);
                //change mouse accordingly
            }

            gui.mouse_focus_mask &= ~(1 << (mb->get_button_index() - 1)); //remove from mask

            if (!gui.mouse_focus) {
                //release event is only sent if a mouse focus (previously pressed button) exists
                return;
            }

            Size2 pos = mpos;

            mb = dynamic_ref_cast<InputEventMouseButton>(mb->xformed_by(Transform2D())); //make a copy
            mb->set_global_position(pos);
            pos = gui.focus_inv_xform.xform(pos);
            mb->set_position(pos);

            Control *mouse_focus = gui.mouse_focus;

            //disable mouse focus if needed before calling input, this makes popups on mouse press event work better, as the release will never be received otherwise
            if (gui.mouse_focus_mask == 0) {
                gui.mouse_focus = nullptr;
            }

            if (mouse_focus && mouse_focus->can_process()) {
                _gui_call_input(mouse_focus, mb);
            }

            /*if (gui.drag_data.get_type()!=VariantType::NIL && mb->get_button_index()==BUTTON_LEFT) {
                _propagate_viewport_notification(this,NOTIFICATION_DRAG_END);
                gui.drag_data=Variant(); //always clear
            }*/

            set_input_as_handled();
        }
    }

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_event);

    if (mm) {

        gui.key_event_accepted = false;
        Point2 mpos = mm->get_position();

        gui.last_mouse_pos = mpos;

        Control *over = nullptr;

        // D&D
        if (!gui.drag_attempted && gui.mouse_focus && mm->get_button_mask() & BUTTON_MASK_LEFT) {

            gui.drag_accum += mm->get_relative();
            float len = gui.drag_accum.length();
            if (len > 10) {

                { //attempt grab, try parent controls too
                    CanvasItem *ci = gui.mouse_focus;
                    while (ci) {

                        Control *control = object_cast<Control>(ci);
                        if (control) {

                            gui.dragging = true;
                            gui.drag_data = control->get_drag_data(control->get_global_transform_with_canvas().affine_inverse().xform(mpos) - gui.drag_accum);
                            if (gui.drag_data.get_type() != VariantType::NIL) {

                                gui.mouse_focus = nullptr;
                                gui.mouse_focus_mask = 0;
                                break;
                            } else {
                                if (gui.drag_preview != nullptr) {
                                    ERR_PRINT("Don't set a drag preview and return null data. Preview was deleted and drag request ignored.");
                                    memdelete(gui.drag_preview);
                                    gui.drag_preview = nullptr;
                                }
                                gui.dragging = false;
                            }

                            if (control->data.mouse_filter == Control::MOUSE_FILTER_STOP)
                                break;
                        }

                        if (ci->is_set_as_toplevel())
                            break;

                        ci = ci->get_parent_item();
                    }
                }

                gui.drag_attempted = true;
                if (gui.drag_data.get_type() != VariantType::NIL) {

                    _propagate_viewport_notification(this, NOTIFICATION_DRAG_BEGIN);
                }
            }
        }

        if (gui.mouse_focus) {
            over = gui.mouse_focus;
            //recompute focus_inv_xform again here

        } else {

            over = _gui_find_control(mpos);
        }

        if (gui.drag_data.get_type() == VariantType::NIL && over && !gui.modal_stack.empty()) {

            Control *top = gui.modal_stack.back();

            if (over != top && !top->is_a_parent_of(over)) {

                PopupMenu *popup_menu = object_cast<PopupMenu>(top);
                MenuButton *popup_menu_parent = nullptr;
                MenuButton *menu_button = object_cast<MenuButton>(over);

                if (popup_menu) {
                    popup_menu_parent = object_cast<MenuButton>(popup_menu->get_parent());
                    if (!popup_menu_parent) {
                        // Go through the parents to see if there's a MenuButton at the end.
                        while (object_cast<PopupMenu>(popup_menu->get_parent())) {
                            popup_menu = object_cast<PopupMenu>(popup_menu->get_parent());
                        }
                        popup_menu_parent = object_cast<MenuButton>(popup_menu->get_parent());
                    }
                }

                // If the mouse is over a menu button, this menu will open automatically
                // if there is already a pop-up menu open at the same hierarchical level.
                if (popup_menu_parent && menu_button && popup_menu_parent->is_switch_on_hover() &&
                        !menu_button->is_disabled() && menu_button->is_switch_on_hover() &&
                        (popup_menu_parent->get_parent()->is_a_parent_of(menu_button) ||
                                menu_button->get_parent()->is_a_parent_of(popup_menu))) {

                    popup_menu->notification(Control::NOTIFICATION_MODAL_CLOSE);
                    popup_menu->_modal_stack_remove();
                    popup_menu->hide();

                    menu_button->pressed();
                } else {
                    over = nullptr; //nothing can be found outside the modal stack
                }
            }
        }

        if (over != gui.mouse_over) {

            if (gui.mouse_over) {
                _gui_call_notification(gui.mouse_over, Control::NOTIFICATION_MOUSE_EXIT);
            }

            _gui_cancel_tooltip();

            if (over) {
                _gui_call_notification(over, Control::NOTIFICATION_MOUSE_ENTER);
            }
        }

        gui.mouse_over = over;

        if (gui.drag_preview) {
            gui.drag_preview->set_position(mpos);
        }

        if (!over) {
            OS::get_singleton()->set_cursor_shape((OS::CursorShape)Input::get_singleton()->get_default_cursor_shape());
            return;
        }

        Transform2D localizer = over->get_global_transform_with_canvas().affine_inverse();
        Size2 pos = localizer.xform(mpos);
        Vector2 speed = localizer.basis_xform(mm->get_speed());
        Vector2 rel = localizer.basis_xform(mm->get_relative());

        mm = dynamic_ref_cast<InputEventMouseMotion>(mm->xformed_by(Transform2D())); //make a copy

        mm->set_global_position(mpos);
        mm->set_speed(speed);
        mm->set_relative(rel);

        if (mm->get_button_mask() == 0) {
            //nothing pressed

            bool can_tooltip = true;

            if (!gui.modal_stack.empty()) {
                if (gui.modal_stack.back() != over && !gui.modal_stack.back()->is_a_parent_of(over))
                    can_tooltip = false;
            }

            bool is_tooltip_shown = false;

            if (gui.tooltip_popup) {
                if (can_tooltip && gui.tooltip) {
                    StringName tooltip = _gui_get_tooltip(over, gui.tooltip->get_global_transform().xform_inv(mpos));

                    if (tooltip.empty())
                        _gui_cancel_tooltip();
                    else if (gui.tooltip_label) {
                        if (tooltip == gui.tooltip_label->get_text()) {
                            is_tooltip_shown = true;
                        }
                    } else if (tooltip == StringName(gui.tooltip_popup->call_va("get_tooltip_text"))) {
                        is_tooltip_shown = true;
                    }
                } else
                    _gui_cancel_tooltip();
            }

            if (can_tooltip && !is_tooltip_shown) {

                gui.tooltip = over;
                gui.tooltip_pos = mpos; //(parent_xform * get_transform()).affine_inverse().xform(pos);
                gui.tooltip_timer = gui.tooltip_delay;
            }
        }

        //pos = gui.focus_inv_xform.xform(pos);

        mm->set_position(pos);

        Control::CursorShape cursor_shape = Control::CURSOR_ARROW;
        {
            Control *c = over;
            Vector2 cpos = pos;
            while (c) {
                cursor_shape = c->get_cursor_shape(cpos);
                cpos = c->get_transform().xform(cpos);
                if (cursor_shape != Control::CURSOR_ARROW)
                    break;
                if (c->data.mouse_filter == Control::MOUSE_FILTER_STOP)
                    break;
                if (c->is_set_as_toplevel())
                    break;
                c = c->get_parent_control();
            }
        }

        OS::get_singleton()->set_cursor_shape((OS::CursorShape)cursor_shape);

        if (over && over->can_process()) {
            _gui_call_input(over, mm);
        }

        set_input_as_handled();

        if (gui.drag_data.get_type() != VariantType::NIL && mm->get_button_mask() & BUTTON_MASK_LEFT) {

            bool can_drop = _gui_drop(over, pos, true);

            if (!can_drop) {
                OS::get_singleton()->set_cursor_shape(OS::CURSOR_FORBIDDEN);
            } else {
                OS::get_singleton()->set_cursor_shape(OS::CURSOR_CAN_DROP);
            }
            //change mouse accordingly i guess
        }
    }

    Ref<InputEventScreenTouch> touch_event = dynamic_ref_cast<InputEventScreenTouch>(p_event);
    if (touch_event) {

        Size2 pos = touch_event->get_position();
        if (touch_event->is_pressed()) {

            Control *over = _gui_find_control(pos);
            if (over) {

                if (!gui.modal_stack.empty()) {

                    Control *top = gui.modal_stack.back();
                    if (over != top && !top->is_a_parent_of(over)) {

                        return;
                    }
                }
                if (over->can_process()) {

                    touch_event = dynamic_ref_cast<InputEventScreenTouch>(touch_event->xformed_by(Transform2D())); //make a copy
                    if (over == gui.mouse_focus) {
                        pos = gui.focus_inv_xform.xform(pos);
                    } else {
                        pos = over->get_global_transform_with_canvas().affine_inverse().xform(pos);
                    }
                    touch_event->set_position(pos);
                    _gui_call_input(over, touch_event);
                }
                set_input_as_handled();
                return;
            }
        } else if (touch_event->get_index() == 0 && gui.last_mouse_focus) {

            if (gui.last_mouse_focus->can_process()) {

                touch_event = dynamic_ref_cast<InputEventScreenTouch>(touch_event->xformed_by(Transform2D())); //make a copy
                touch_event->set_position(gui.focus_inv_xform.xform(pos));

                _gui_call_input(gui.last_mouse_focus, touch_event);
            }
            set_input_as_handled();
            return;
        }
    }

    Ref<InputEventGesture> gesture_event = dynamic_ref_cast<InputEventGesture>(p_event);
    if (gesture_event) {

        gui.key_event_accepted = false;

        _gui_cancel_tooltip();

        Size2 pos = gesture_event->get_position();

        Control *over = _gui_find_control(pos);
        if (over) {

            if (over->can_process()) {

                gesture_event = dynamic_ref_cast<InputEventGesture>(gesture_event->xformed_by(Transform2D())); //make a copy
                if (over == gui.mouse_focus) {
                    pos = gui.focus_inv_xform.xform(pos);
                } else {
                    pos = over->get_global_transform_with_canvas().affine_inverse().xform(pos);
                }
                gesture_event->set_position(pos);
                _gui_call_input(over, gesture_event);
            }
            set_input_as_handled();
            return;
        }
    }

    Ref<InputEventScreenDrag> drag_event = dynamic_ref_cast<InputEventScreenDrag>(p_event);
    if (drag_event) {

        Control *over = gui.mouse_focus;
        if (!over) {
            over = _gui_find_control(drag_event->get_position());
        }
        if (over) {

            if (!gui.modal_stack.empty()) {

                Control *top = gui.modal_stack.back();
                if (over != top && !top->is_a_parent_of(over)) {

                    return;
                }
            }
            if (over->can_process()) {

                Transform2D localizer = over->get_global_transform_with_canvas().affine_inverse();
                Size2 pos = localizer.xform(drag_event->get_position());
                Vector2 speed = localizer.basis_xform(drag_event->get_speed());
                Vector2 rel = localizer.basis_xform(drag_event->get_relative());

                drag_event = dynamic_ref_cast<InputEventScreenDrag>(drag_event->xformed_by(Transform2D())); //make a copy

                drag_event->set_speed(speed);
                drag_event->set_relative(rel);
                drag_event->set_position(pos);

                _gui_call_input(over, drag_event);
            }

            set_input_as_handled();
            return;
        }
    }

    if (not mm && not mb && p_event->is_action_type()) {

        if (gui.key_focus && !gui.key_focus->is_visible_in_tree()) {
            gui.key_focus->release_focus();
        }

        if (gui.key_focus) {

            gui.key_event_accepted = false;
            if (gui.key_focus->can_process()) {
                gui.key_focus->call_multilevel(SceneStringNames::get_singleton()->_gui_input, p_event);
                if (gui.key_focus) //maybe lost it
                    gui.key_focus->emit_signal(SceneStringNames::get_singleton()->gui_input, p_event);
            }

            if (gui.key_event_accepted) {

                set_input_as_handled();
                return;
            }
        }

        if (p_event->is_pressed() && p_event->is_action("ui_cancel") && !gui.modal_stack.empty()) {

            _gui_sort_modal_stack();
            Control *top = gui.modal_stack.back();
            if (!top->data.modal_exclusive) {

                top->notification(Control::NOTIFICATION_MODAL_CLOSE);
                top->_modal_stack_remove();
                top->hide();
                // Close modal, set input as handled
                set_input_as_handled();
                return;
            }
        }

        Control *from = gui.key_focus ? gui.key_focus : nullptr; //hmm

        //keyboard focus
        //if (from && p_event->is_pressed() && !p_event->get_alt() && !p_event->get_metakey() && !p_event->key->get_command()) {

        Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_event);
        //need to check for mods, otherwise any combination of alt/ctrl/shift+<up/down/left/righ/etc> is handled here when it shouldn't be.
        bool mods = k && (k->get_control() || k->get_alt() || k->get_shift() || k->get_metakey());

        if (from && p_event->is_pressed()) {
            Control *next = nullptr;

            Input *input = Input::get_singleton();

            if (p_event->is_action_pressed("ui_focus_next") && input->is_action_just_pressed("ui_focus_next")) {

                next = from->find_next_valid_focus();
            }

            if (p_event->is_action_pressed("ui_focus_prev") && input->is_action_just_pressed("ui_focus_prev")) {

                next = from->find_prev_valid_focus();
            }

            if (!mods && p_event->is_action_pressed("ui_up") && input->is_action_just_pressed("ui_up")) {

                next = from->_get_focus_neighbour(Margin::Top);
            }

            if (!mods && p_event->is_action_pressed("ui_left") && input->is_action_just_pressed("ui_left")) {

                next = from->_get_focus_neighbour(Margin::Left);
            }

            if (!mods && p_event->is_action_pressed("ui_right") && input->is_action_just_pressed("ui_right")) {

                next = from->_get_focus_neighbour(Margin::Right);
            }

            if (!mods && p_event->is_action_pressed("ui_down") && input->is_action_just_pressed("ui_down")) {

                next = from->_get_focus_neighbour(Margin::Bottom);
            }

            if (next) {
                next->grab_focus();
                set_input_as_handled();
            }
        }
    }
}

void Viewport::_gui_add_root_control(Control *p_control) {

    gui.roots_order_dirty = true;
    gui.roots.emplace_back(p_control);
}

void Viewport::_gui_add_subwindow_control(Control *p_control) {

    p_control->connect("visibility_changed", this, "_subwindow_visibility_changed");

    if (p_control->is_visible_in_tree()) {
        gui.subwindow_order_dirty = true;
        gui.subwindows.push_back(p_control);
    }

    gui.all_known_subwindows.emplace_back(p_control);
}

void Viewport::_gui_set_subwindow_order_dirty() {
    gui.subwindow_order_dirty = true;
}

void Viewport::_gui_set_root_order_dirty() {
    gui.roots_order_dirty = true;
}

void Viewport::_gui_remove_modal_control(Control* MI) {

    gui.modal_stack.erase_first(MI);
}

void Viewport::_gui_remove_from_modal_stack(Control* MI, ObjectID p_prev_focus_owner) {

    //transfer the focus stack to the next

    auto next = gui.modal_stack.erase_first(MI);

    if (p_prev_focus_owner.is_valid()) {

        // for previous window in stack, pass the focus so it feels more
        // natural

        if (next == gui.modal_stack.end()) { //top of stack

            Object *pfo = gObjectDB().get_instance(p_prev_focus_owner);
            Control *pfoc = object_cast<Control>(pfo);
            if (!pfoc)
                return;

            if (!pfoc->is_inside_tree() || !pfoc->is_visible_in_tree())
                return;
            pfoc->grab_focus();
        } else {

            (*next)->_modal_set_prev_focus_owner(p_prev_focus_owner);
        }
    }
}

void Viewport::_gui_force_drag(Control *p_base, const Variant &p_data, Control *p_control) {

    ERR_FAIL_COND_MSG(p_data.get_type() == VariantType::NIL, "Drag data must be a value.");

    gui.dragging = true;
    gui.drag_data = p_data;
    gui.mouse_focus = nullptr;

    if (p_control) {
        _gui_set_drag_preview(p_base, p_control);
    }
}

void Viewport::_gui_set_drag_preview(Control *p_base, Control *p_control) {

    ERR_FAIL_NULL(p_control);
    ERR_FAIL_COND(!object_cast<Control>((Object *)p_control));
    ERR_FAIL_COND(p_control->is_inside_tree());
    ERR_FAIL_COND(p_control->get_parent() != nullptr);

    if (gui.drag_preview) {
        memdelete(gui.drag_preview);
    }
    p_control->set_as_toplevel(true);
    p_control->set_position(gui.last_mouse_pos);
    p_base->get_root_parent_control()->add_child(p_control); //add as child of viewport
    p_control->raise();

    gui.drag_preview = p_control;
}

void Viewport::_gui_remove_root_control(Control * RI) {

    gui.roots.erase_first(RI);
}

void Viewport::_gui_remove_subwindow_control(Control* SI) {

    ERR_FAIL_COND(SI==nullptr);

    SI->disconnect("visibility_changed", this, "_subwindow_visibility_changed");

    auto E = eastl::find(gui.subwindows.begin(), gui.subwindows.end(),SI);
    if (E!= gui.subwindows.end())
        gui.subwindows.erase(E);

    gui.all_known_subwindows.erase_first(SI);
}

void Viewport::_gui_unfocus_control(Control *p_control) {

    if (gui.key_focus == p_control) {
        gui.key_focus->release_focus();
    }
}

void Viewport::_gui_hid_control(Control *p_control) {

    if (gui.mouse_focus == p_control) {
        _drop_mouse_focus();
    }

    /* ???
    if (data.window==p_control) {
        window->drag_data=Variant();
        if (window->drag_preview) {
            memdelete( window->drag_preview);
            window->drag_preview=NULL;
        }
    }
    */

    if (gui.key_focus == p_control)
        _gui_remove_focus();
    if (gui.mouse_over == p_control)
        gui.mouse_over = nullptr;
    if (gui.tooltip == p_control)
        _gui_cancel_tooltip();
}

void Viewport::_gui_remove_control(Control *p_control) {

    if (gui.mouse_focus == p_control) {
        gui.mouse_focus = nullptr;
        gui.mouse_focus_mask = 0;
    }
    if (gui.last_mouse_focus == p_control) {
        gui.last_mouse_focus = nullptr;
    }
    if (gui.key_focus == p_control)
        gui.key_focus = nullptr;
    if (gui.mouse_over == p_control)
        gui.mouse_over = nullptr;
    if (gui.tooltip == p_control)
        gui.tooltip = nullptr;
    if (gui.tooltip_popup == p_control) {
        _gui_cancel_tooltip();
    }
}

void Viewport::_gui_remove_focus() {

    if (gui.key_focus) {
        Node *f = gui.key_focus;
        gui.key_focus = nullptr;
        f->notification(Control::NOTIFICATION_FOCUS_EXIT, true);
    }
}

bool Viewport::_gui_is_modal_on_top(const Control *p_control) {

    return (!gui.modal_stack.empty() && gui.modal_stack.back() == p_control);
}

bool Viewport::_gui_control_has_focus(const Control *p_control) {

    return gui.key_focus == p_control;
}

void Viewport::_gui_control_grab_focus(Control *p_control) {

    //no need for change
    if (gui.key_focus && gui.key_focus == p_control)
        return;
    get_tree()->call_group_flags(SceneTree::GROUP_CALL_REALTIME, "_viewports", "_gui_remove_focus");
    gui.key_focus = p_control;
    emit_signal("gui_focus_changed", Variant(p_control));
    p_control->notification(Control::NOTIFICATION_FOCUS_ENTER);
    p_control->update();
}

void Viewport::_gui_accept_event() {

    gui.key_event_accepted = true;
    if (is_inside_tree())
        set_input_as_handled();
}

void Viewport::_drop_mouse_focus() {

    Control *c = gui.mouse_focus;
    int mask = gui.mouse_focus_mask;
    gui.mouse_focus = nullptr;
    gui.mouse_focus_mask = 0;

    for (int i = 0; i < 3; i++) {

        if (mask & (1 << i)) {
            Ref<InputEventMouseButton> mb(make_ref_counted<InputEventMouseButton>());
            mb->set_position(c->get_local_mouse_position());
            mb->set_global_position(c->get_local_mouse_position());
            mb->set_button_index(i + 1);
            mb->set_pressed(false);
            c->call_multilevel(SceneStringNames::get_singleton()->_gui_input, mb);
        }
    }
}

void Viewport::_drop_physics_mouseover() {

    physics_has_last_mousepos = false;

    while (!physics_2d_mouseover.empty()) {
        Object *o = gObjectDB().get_instance(physics_2d_mouseover.begin()->first);
        if (o) {
            CollisionObject2D *co = object_cast<CollisionObject2D>(o);
            co->_mouse_exit();
        }
        physics_2d_mouseover.erase(physics_2d_mouseover.begin());
    }

#ifndef _3D_DISABLED
    if (physics_object_over.is_valid()) {
        CollisionObject3D *co = object_cast<CollisionObject3D>(gObjectDB().get_instance(physics_object_over));
        if (co) {
            co->_mouse_exit();
        }
    }

    physics_object_over = physics_object_capture = 0;
#endif
}

void Viewport::_gui_show_modal(Control *p_control) {

    gui.modal_stack.emplace_back(p_control);
    if (gui.key_focus)
        p_control->_modal_set_prev_focus_owner(gui.key_focus->get_instance_id());
    else
        p_control->_modal_set_prev_focus_owner(ObjectID(0ULL));

    if (gui.mouse_focus && !p_control->is_a_parent_of(gui.mouse_focus) && !gui.mouse_click_grabber) {

        _drop_mouse_focus();
    }
}

Control *Viewport::_gui_get_focus_owner() {

    return gui.key_focus;
}

void Viewport::_gui_grab_click_focus(Control *p_control) {

    gui.mouse_click_grabber = p_control;
    call_deferred("_post_gui_grab_click_focus");
}

void Viewport::_post_gui_grab_click_focus() {

    Control *focus_grabber = gui.mouse_click_grabber;
    if (!focus_grabber) {
        // Redundant grab requests were made
        return;
    }
    gui.mouse_click_grabber = nullptr;

    if (gui.mouse_focus) {

        if (gui.mouse_focus == focus_grabber)
            return;

        int mask = gui.mouse_focus_mask;
        Point2 click = gui.mouse_focus->get_global_transform_with_canvas().affine_inverse().xform(gui.last_mouse_pos);

        for (int i = 0; i < 3; i++) {

            if (mask & (1 << i)) {

                Ref<InputEventMouseButton> mb(make_ref_counted<InputEventMouseButton>());

                //send unclic

                mb->set_position(click);
                mb->set_button_index(i + 1);
                mb->set_pressed(false);
                gui.mouse_focus->call_multilevel(SceneStringNames::get_singleton()->_gui_input, mb);
            }
        }

        gui.mouse_focus = focus_grabber;
        gui.focus_inv_xform = gui.mouse_focus->get_global_transform_with_canvas().affine_inverse();
        click = gui.mouse_focus->get_global_transform_with_canvas().affine_inverse().xform(gui.last_mouse_pos);

        for (int i = 0; i < 3; i++) {

            if (mask & (1 << i)) {

                Ref<InputEventMouseButton> mb(make_ref_counted<InputEventMouseButton>());

                //send clic

                mb->set_position(click);
                mb->set_button_index(i + 1);
                mb->set_pressed(true);
                gui.mouse_focus->call_deferred(SceneStringNames::get_singleton()->_gui_input, mb);
            }
        }
    }
}

///////////////////////////////

void Viewport::input(const Ref<InputEvent> &p_event) {

    ERR_FAIL_COND(!is_inside_tree());

    local_input_handled = false;

    if (!is_input_handled()) {
        get_tree()->_call_input_pause(input_group, "_input", p_event); //not a bug, must happen before GUI, order is _input -> gui input -> _unhandled input
    }

    if (!is_input_handled()) {
        _gui_input_event(p_event);
    }
    //get_tree()->call_group(SceneTree::GROUP_CALL_REVERSE|SceneTree::GROUP_CALL_REALTIME|SceneTree::GROUP_CALL_MULIILEVEL,gui_input_group,"_gui_input",p_event); //special one for GUI, as controls use their own process check
}

void Viewport::unhandled_input(const Ref<InputEvent> &p_event) {

    ERR_FAIL_COND(!is_inside_tree());

    get_tree()->_call_input_pause(unhandled_input_group, "_unhandled_input", p_event);
    //call_group(GROUP_CALL_REVERSE|GROUP_CALL_REALTIME|GROUP_CALL_MULIILEVEL,"unhandled_input","_unhandled_input",ev);
    if (!get_tree()->input_handled && object_cast<InputEventKey>(p_event.get()) != nullptr) {
        get_tree()->_call_input_pause(unhandled_key_input_group, "_unhandled_key_input", p_event);
        //call_group(GROUP_CALL_REVERSE|GROUP_CALL_REALTIME|GROUP_CALL_MULIILEVEL,"unhandled_key_input","_unhandled_key_input",ev);
    }

    if (physics_object_picking && !get_tree()->input_handled) {

        if (Input::get_singleton()->get_mouse_mode() != Input::MOUSE_MODE_CAPTURED &&
                (object_cast<InputEventMouseButton>(p_event.get()) ||
                        object_cast<InputEventMouseMotion>(p_event.get()) ||
                        object_cast<InputEventScreenDrag>(p_event.get()) ||
                        object_cast<InputEventScreenTouch>(p_event.get()) ||
                        object_cast<InputEventKey>(p_event.get()) //to remember state

                        )) {
            physics_picking_events.push_back(p_event);
        }
    }
}

void Viewport::set_use_own_world(bool p_world) {

    if (p_world == (own_world!=nullptr))
        return;

    if (is_inside_tree())
        _propagate_exit_world(this);

    if (!p_world) {
        own_world = Ref<World3D>();
        if (world) {
            world->disconnect(CoreStringNames::get_singleton()->changed, this, "_own_world_changed");
        }
    } else {
        if (world) {
            own_world = dynamic_ref_cast<World3D>(world->duplicate());
            world->connect(CoreStringNames::get_singleton()->changed, this, "_own_world_changed");
        } else {
            own_world = make_ref_counted<World3D>();
        }
    }

    if (is_inside_tree())
        _propagate_enter_world(this);

    if (is_inside_tree()) {
        RenderingServer::get_singleton()->viewport_set_scenario(viewport, find_world()->get_scenario());
    }

    _update_listener();
}

bool Viewport::is_using_own_world() const {

    return own_world;
}

void Viewport::set_attach_to_screen_rect(const Rect2 &p_rect) {

    RenderingServer::get_singleton()->viewport_attach_to_screen(viewport, p_rect);
    to_screen_rect = p_rect;
}

Rect2 Viewport::get_attach_to_screen_rect() const {

    return to_screen_rect;
}

void Viewport::set_use_render_direct_to_screen(bool p_render_direct_to_screen) {

    if (p_render_direct_to_screen == render_direct_to_screen)
        return;

    render_direct_to_screen = p_render_direct_to_screen;
    RenderingServer::get_singleton()->viewport_set_render_direct_to_screen(viewport, p_render_direct_to_screen);
}

bool Viewport::is_using_render_direct_to_screen() const {
    return render_direct_to_screen;
}

void Viewport::set_physics_object_picking(bool p_enable) {

    physics_object_picking = p_enable;
    if (!physics_object_picking) {
        physics_picking_events.clear();
    }
}

bool Viewport::get_physics_object_picking() {

    return physics_object_picking;
}

Vector2 Viewport::get_camera_coords(const Vector2 &p_viewport_coords) const {

    Transform2D xf = get_final_transform();
    return xf.xform(p_viewport_coords);
}

Vector2 Viewport::get_camera_rect_size() const {

    return size;
}

bool Viewport::gui_has_modal_stack() const {

    return !gui.modal_stack.empty();
}

void Viewport::set_disable_input(bool p_disable) {
    disable_input = p_disable;
}

bool Viewport::is_input_disabled() const {

    return disable_input;
}

void Viewport::set_disable_3d(bool p_disable) {
    disable_3d = p_disable;
    RenderingServer::get_singleton()->viewport_set_disable_3d(viewport, p_disable);
}

bool Viewport::is_3d_disabled() const {

    return disable_3d;
}

void Viewport::set_keep_3d_linear(bool p_keep_3d_linear) {
    keep_3d_linear = p_keep_3d_linear;
    RenderingServer::get_singleton()->viewport_set_keep_3d_linear(viewport, keep_3d_linear);
}

bool Viewport::get_keep_3d_linear() const {

    return keep_3d_linear;
}

Variant Viewport::gui_get_drag_data() const {
    return gui.drag_data;
}

Control *Viewport::get_modal_stack_top() const {
    return !gui.modal_stack.empty() ? gui.modal_stack.back() : nullptr;
}

StringName Viewport::get_configuration_warning() const {

    /*if (get_parent() && !object_cast<Control>(get_parent()) && !render_target) {

        return TTR("This viewport is not set as render target. If you intend for it to display its contents directly to the screen, make it a child of a Control so it can obtain a size. Otherwise, make it a RenderTarget and assign its internal texture to some node for display.");
    }*/
    if (size.x == 0 || size.y == 0) {
        return TTR("Viewport size must be greater than 0 to render anything.");
    }

    return StringName();
}

void Viewport::gui_reset_canvas_sort_index() {
    gui.canvas_sort_index = 0;
}
int Viewport::gui_get_canvas_sort_index() {

    return gui.canvas_sort_index++;
}

void Viewport::set_msaa(MSAA p_msaa) {

    ERR_FAIL_INDEX(p_msaa, MSAA_COUNT);
    if (msaa == p_msaa)
        return;
    msaa = p_msaa;
    RenderingServer::get_singleton()->viewport_set_msaa(viewport, RS::ViewportMSAA(p_msaa));
}

Viewport::MSAA Viewport::get_msaa() const {

    return msaa;
}

void Viewport::set_hdr(bool p_hdr) {

    if (hdr == p_hdr)
        return;

    hdr = p_hdr;
    RenderingServer::get_singleton()->viewport_set_hdr(viewport, p_hdr);
}

bool Viewport::get_hdr() const {

    return hdr;
}

void Viewport::set_usage(Usage p_usage) {

    usage = p_usage;
    RenderingServer::get_singleton()->viewport_set_usage(viewport, RS::ViewportUsage(p_usage));
}

Viewport::Usage Viewport::get_usage() const {
    return usage;
}

void Viewport::set_debug_draw(DebugDraw p_debug_draw) {

    debug_draw = p_debug_draw;
    RenderingServer::get_singleton()->viewport_set_debug_draw(viewport, RS::ViewportDebugDraw(p_debug_draw));
}

Viewport::DebugDraw Viewport::get_debug_draw() const {

    return debug_draw;
}

int Viewport::get_render_info(RenderInfo p_info) {

    return RenderingServer::get_singleton()->viewport_get_render_info(viewport, RS::ViewportRenderInfo(p_info));
}

void Viewport::set_snap_controls_to_pixels(bool p_enable) {

    snap_controls_to_pixels = p_enable;
}

bool Viewport::is_snap_controls_to_pixels_enabled() const {

    return snap_controls_to_pixels;
}

bool Viewport::gui_is_dragging() const {
    return gui.dragging;
}

void Viewport::set_input_as_handled() {
    _drop_physics_mouseover();
    if (handle_input_locally) {
        local_input_handled = true;
    } else {
        ERR_FAIL_COND(!is_inside_tree());
        get_tree()->set_input_as_handled();
    }
}

bool Viewport::is_input_handled() const {
    if (handle_input_locally) {
        return local_input_handled;
    } else {
        ERR_FAIL_COND_V(!is_inside_tree(), false);
        return get_tree()->is_input_handled();
    }
}

void Viewport::set_handle_input_locally(bool p_enable) {
    handle_input_locally = p_enable;
}

bool Viewport::is_handling_input_locally() const {
    return handle_input_locally;
}

void Viewport::_validate_property(PropertyInfo &property) const {

    //if (RenderingServer::get_singleton()->is_low_end() && property.name == "hdr") {
    //    property.usage = PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL;
    //}
}

void Viewport::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_use_arvr", {"use"}), &Viewport::set_use_arvr);
    MethodBinder::bind_method(D_METHOD("use_arvr"), &Viewport::use_arvr);

    MethodBinder::bind_method(D_METHOD("set_size", {"size"}), &Viewport::set_size);
    MethodBinder::bind_method(D_METHOD("get_size"), &Viewport::get_size);
    MethodBinder::bind_method(D_METHOD("set_world_2d", {"world_2d"}), &Viewport::set_world_2d);
    MethodBinder::bind_method(D_METHOD("get_world_2d"), &Viewport::get_world_2d);
    MethodBinder::bind_method(D_METHOD("find_world_2d"), &Viewport::find_world_2d);
    MethodBinder::bind_method(D_METHOD("set_world", {"world"}), &Viewport::set_world);
    MethodBinder::bind_method(D_METHOD("get_world"), &Viewport::get_world);
    MethodBinder::bind_method(D_METHOD("find_world"), &Viewport::find_world);

    MethodBinder::bind_method(D_METHOD("set_canvas_transform", {"xform"}), &Viewport::set_canvas_transform);
    MethodBinder::bind_method(D_METHOD("get_canvas_transform"), &Viewport::get_canvas_transform);

    MethodBinder::bind_method(D_METHOD("set_global_canvas_transform", {"xform"}), &Viewport::set_global_canvas_transform);
    MethodBinder::bind_method(D_METHOD("get_global_canvas_transform"), &Viewport::get_global_canvas_transform);
    MethodBinder::bind_method(D_METHOD("get_final_transform"), &Viewport::get_final_transform);

    MethodBinder::bind_method(D_METHOD("get_visible_rect"), &Viewport::get_visible_rect);
    MethodBinder::bind_method(D_METHOD("set_transparent_background", {"enable"}), &Viewport::set_transparent_background);
    MethodBinder::bind_method(D_METHOD("has_transparent_background"), &Viewport::has_transparent_background);

    MethodBinder::bind_method(D_METHOD("_vp_input"), &Viewport::_vp_input);
    MethodBinder::bind_method(D_METHOD("_vp_input_text", {"text"}), &Viewport::_vp_input_text);
    MethodBinder::bind_method(D_METHOD("_vp_unhandled_input"), &Viewport::_vp_unhandled_input);

    MethodBinder::bind_method(D_METHOD("set_size_override", {"enable", "size", "margin"}), &Viewport::set_size_override, {DEFVAL(Size2(-1, -1)), DEFVAL(Size2(0, 0))});
    MethodBinder::bind_method(D_METHOD("get_size_override"), &Viewport::get_size_override);
    MethodBinder::bind_method(D_METHOD("is_size_override_enabled"), &Viewport::is_size_override_enabled);
    MethodBinder::bind_method(D_METHOD("set_size_override_stretch", {"enabled"}), &Viewport::set_size_override_stretch);
    MethodBinder::bind_method(D_METHOD("is_size_override_stretch_enabled"), &Viewport::is_size_override_stretch_enabled);

    MethodBinder::bind_method(D_METHOD("set_vflip", {"enable"}), &Viewport::set_vflip);
    MethodBinder::bind_method(D_METHOD("get_vflip"), &Viewport::get_vflip);

    MethodBinder::bind_method(D_METHOD("set_clear_mode", {"mode"}), &Viewport::set_clear_mode);
    MethodBinder::bind_method(D_METHOD("get_clear_mode"), &Viewport::get_clear_mode);

    MethodBinder::bind_method(D_METHOD("set_update_mode", {"mode"}), &Viewport::set_update_mode);
    MethodBinder::bind_method(D_METHOD("get_update_mode"), &Viewport::get_update_mode);

    MethodBinder::bind_method(D_METHOD("set_msaa", {"msaa"}), &Viewport::set_msaa);
    MethodBinder::bind_method(D_METHOD("get_msaa"), &Viewport::get_msaa);

    MethodBinder::bind_method(D_METHOD("set_hdr", {"enable"}), &Viewport::set_hdr);
    MethodBinder::bind_method(D_METHOD("get_hdr"), &Viewport::get_hdr);

    MethodBinder::bind_method(D_METHOD("set_usage", {"usage"}), &Viewport::set_usage);
    MethodBinder::bind_method(D_METHOD("get_usage"), &Viewport::get_usage);

    MethodBinder::bind_method(D_METHOD("set_debug_draw", {"debug_draw"}), &Viewport::set_debug_draw);
    MethodBinder::bind_method(D_METHOD("get_debug_draw"), &Viewport::get_debug_draw);

    MethodBinder::bind_method(D_METHOD("get_render_info", {"info"}), &Viewport::get_render_info);

    MethodBinder::bind_method(D_METHOD("get_texture"), &Viewport::get_texture);

    MethodBinder::bind_method(D_METHOD("set_physics_object_picking", {"enable"}), &Viewport::set_physics_object_picking);
    MethodBinder::bind_method(D_METHOD("get_physics_object_picking"), &Viewport::get_physics_object_picking);

    MethodBinder::bind_method(D_METHOD("get_viewport_rid"), &Viewport::get_viewport_rid);
    MethodBinder::bind_method(D_METHOD("input", {"local_event"}), &Viewport::input);
    MethodBinder::bind_method(D_METHOD("unhandled_input", {"local_event"}), &Viewport::unhandled_input);

    MethodBinder::bind_method(D_METHOD("update_worlds"), &Viewport::update_worlds);

    MethodBinder::bind_method(D_METHOD("set_use_own_world", {"enable"}), &Viewport::set_use_own_world);
    MethodBinder::bind_method(D_METHOD("is_using_own_world"), &Viewport::is_using_own_world);

    MethodBinder::bind_method(D_METHOD("get_camera"), &Viewport::get_camera);

    MethodBinder::bind_method(D_METHOD("set_as_audio_listener", {"enable"}), &Viewport::set_as_audio_listener);
    MethodBinder::bind_method(D_METHOD("is_audio_listener"), &Viewport::is_audio_listener);

    MethodBinder::bind_method(D_METHOD("set_as_audio_listener_2d", {"enable"}), &Viewport::set_as_audio_listener_2d);
    MethodBinder::bind_method(D_METHOD("is_audio_listener_2d"), &Viewport::is_audio_listener_2d);
    MethodBinder::bind_method(D_METHOD("set_attach_to_screen_rect", {"rect"}), &Viewport::set_attach_to_screen_rect);
    MethodBinder::bind_method(D_METHOD("set_use_render_direct_to_screen", {"enable"}), &Viewport::set_use_render_direct_to_screen);
    MethodBinder::bind_method(D_METHOD("is_using_render_direct_to_screen"), &Viewport::is_using_render_direct_to_screen);

    MethodBinder::bind_method(D_METHOD("get_mouse_position"), &Viewport::get_mouse_position);
    MethodBinder::bind_method(D_METHOD("warp_mouse", {"to_position"}), &Viewport::warp_mouse);

    MethodBinder::bind_method(D_METHOD("gui_has_modal_stack"), &Viewport::gui_has_modal_stack);
    MethodBinder::bind_method(D_METHOD("gui_get_drag_data"), &Viewport::gui_get_drag_data);
    MethodBinder::bind_method(D_METHOD("gui_is_dragging"), &Viewport::gui_is_dragging);

    MethodBinder::bind_method(D_METHOD("get_modal_stack_top"), &Viewport::get_modal_stack_top);

    MethodBinder::bind_method(D_METHOD("set_disable_input", {"disable"}), &Viewport::set_disable_input);
    MethodBinder::bind_method(D_METHOD("is_input_disabled"), &Viewport::is_input_disabled);

    MethodBinder::bind_method(D_METHOD("set_disable_3d", {"disable"}), &Viewport::set_disable_3d);
    MethodBinder::bind_method(D_METHOD("is_3d_disabled"), &Viewport::is_3d_disabled);

    MethodBinder::bind_method(D_METHOD("set_keep_3d_linear", {"keep_3d_linear"}), &Viewport::set_keep_3d_linear);
    MethodBinder::bind_method(D_METHOD("get_keep_3d_linear"), &Viewport::get_keep_3d_linear);

    MethodBinder::bind_method(D_METHOD("_gui_show_tooltip"), &Viewport::_gui_show_tooltip);
    MethodBinder::bind_method(D_METHOD("_gui_remove_focus"), &Viewport::_gui_remove_focus);
    MethodBinder::bind_method(D_METHOD("_post_gui_grab_click_focus"), &Viewport::_post_gui_grab_click_focus);

    MethodBinder::bind_method(D_METHOD("set_shadow_atlas_size", {"size"}), &Viewport::set_shadow_atlas_size);
    MethodBinder::bind_method(D_METHOD("get_shadow_atlas_size"), &Viewport::get_shadow_atlas_size);

    MethodBinder::bind_method(D_METHOD("set_snap_controls_to_pixels", {"enabled"}), &Viewport::set_snap_controls_to_pixels);
    MethodBinder::bind_method(D_METHOD("is_snap_controls_to_pixels_enabled"), &Viewport::is_snap_controls_to_pixels_enabled);

    MethodBinder::bind_method(D_METHOD("set_shadow_atlas_quadrant_subdiv", {"quadrant", "subdiv"}), &Viewport::set_shadow_atlas_quadrant_subdiv);
    MethodBinder::bind_method(D_METHOD("get_shadow_atlas_quadrant_subdiv", {"quadrant"}), &Viewport::get_shadow_atlas_quadrant_subdiv);

    MethodBinder::bind_method(D_METHOD("set_input_as_handled"), &Viewport::set_input_as_handled);
    MethodBinder::bind_method(D_METHOD("is_input_handled"), &Viewport::is_input_handled);

    MethodBinder::bind_method(D_METHOD("set_handle_input_locally", {"enable"}), &Viewport::set_handle_input_locally);
    MethodBinder::bind_method(D_METHOD("is_handling_input_locally"), &Viewport::is_handling_input_locally);

    MethodBinder::bind_method(D_METHOD("_subwindow_visibility_changed"), &Viewport::_subwindow_visibility_changed);

    MethodBinder::bind_method(D_METHOD("_own_world_changed"), &Viewport::_own_world_changed);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "arvr"), "set_use_arvr", "use_arvr");

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "size"), "set_size", "get_size");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "size_override_stretch"), "set_size_override_stretch", "is_size_override_stretch_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "own_world"), "set_use_own_world", "is_using_own_world");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "world", PropertyHint::ResourceType, "World3D"), "set_world", "get_world");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "world_2d", PropertyHint::ResourceType, "World2D", 0), "set_world_2d", "get_world_2d");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "transparent_bg"), "set_transparent_background", "has_transparent_background");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "handle_input_locally"), "set_handle_input_locally", "is_handling_input_locally");
    ADD_GROUP("Rendering", "rnd_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "rnd_msaa", PropertyHint::Enum, "Disabled,2x,4x,8x,16x,AndroidVR 2x,AndroidVR 4x"), "set_msaa", "get_msaa");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "rnd_hdr"), "set_hdr", "get_hdr");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "rnd_disable_3d"), "set_disable_3d", "is_3d_disabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "rnd_keep_3d_linear"), "set_keep_3d_linear", "get_keep_3d_linear");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "rnd_usage", PropertyHint::Enum, "2D,2D No-Sampling,3D,3D No-Effects"), "set_usage", "get_usage");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "rnd_render_direct_to_screen"), "set_use_render_direct_to_screen", "is_using_render_direct_to_screen");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "rnd_debug_draw", PropertyHint::Enum, "Disabled,Unshaded,Overdraw,Wireframe"), "set_debug_draw", "get_debug_draw");
    ADD_GROUP("Render Target", "render_target_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "render_target_v_flip"), "set_vflip", "get_vflip");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "render_target_clear_mode", PropertyHint::Enum, "Always,Never,Next Frame"), "set_clear_mode", "get_clear_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "render_target_update_mode", PropertyHint::Enum, "Disabled,Once,When Visible,Always"), "set_update_mode", "get_update_mode");
    ADD_GROUP("Audio Listener3D", "audio_listener_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "audio_listener_enable_2d"), "set_as_audio_listener_2d", "is_audio_listener_2d");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "audio_listener_enable_3d"), "set_as_audio_listener", "is_audio_listener");
    ADD_GROUP("Physics", "physics_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "physics_object_picking"), "set_physics_object_picking", "get_physics_object_picking");
    ADD_GROUP("GUI", "gui_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "gui_disable_input"), "set_disable_input", "is_input_disabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "gui_snap_controls_to_pixels"), "set_snap_controls_to_pixels", "is_snap_controls_to_pixels_enabled");
    ADD_GROUP("Shadow Atlas", "shadow_atlas_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "shadow_atlas_size"), "set_shadow_atlas_size", "get_shadow_atlas_size");
    ADD_PROPERTY_ARRAY("Shadow Atlas Quads",4,"shadow_atlas_quad");
    for (int i = 0; i < 4; ++i)
        ADD_PROPERTYI(
                PropertyInfo(VariantType::INT, StringName("shadow_atlas_quad/" + itos(i) + "/subdiv"),
                             PropertyHint::Enum,
                             "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"),
                             "set_shadow_atlas_quadrant_subdiv", "get_shadow_atlas_quadrant_subdiv", i);

    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM2D, "canvas_transform", PropertyHint::None, "", 0), "set_canvas_transform", "get_canvas_transform");
    ADD_PROPERTY(PropertyInfo(VariantType::TRANSFORM2D, "global_canvas_transform", PropertyHint::None, "", 0), "set_global_canvas_transform", "get_global_canvas_transform");

    ADD_SIGNAL(MethodInfo("size_changed"));
    ADD_SIGNAL(MethodInfo("gui_focus_changed", PropertyInfo(VariantType::OBJECT, "node", PropertyHint::ResourceType, "Control")));

    BIND_ENUM_CONSTANT(UPDATE_DISABLED);
    BIND_ENUM_CONSTANT(UPDATE_ONCE);
    BIND_ENUM_CONSTANT(UPDATE_WHEN_VISIBLE);
    BIND_ENUM_CONSTANT(UPDATE_ALWAYS);

    BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
    BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_1);
    BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_4);
    BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_16);
    BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_64);
    BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_256);
    BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_1024);
    BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_MAX);

    BIND_ENUM_CONSTANT(RENDER_INFO_OBJECTS_IN_FRAME);
    BIND_ENUM_CONSTANT(RENDER_INFO_VERTICES_IN_FRAME);
    BIND_ENUM_CONSTANT(RENDER_INFO_MATERIAL_CHANGES_IN_FRAME);
    BIND_ENUM_CONSTANT(RENDER_INFO_SHADER_CHANGES_IN_FRAME);
    BIND_ENUM_CONSTANT(RENDER_INFO_SURFACE_CHANGES_IN_FRAME);
    BIND_ENUM_CONSTANT(RENDER_INFO_DRAW_CALLS_IN_FRAME);
    BIND_ENUM_CONSTANT(RENDER_INFO_2D_ITEMS_IN_FRAME);
    BIND_ENUM_CONSTANT(RENDER_INFO_2D_DRAW_CALLS_IN_FRAME);
    BIND_ENUM_CONSTANT(RENDER_INFO_MAX);

    BIND_ENUM_CONSTANT(DEBUG_DRAW_DISABLED);
    BIND_ENUM_CONSTANT(DEBUG_DRAW_UNSHADED);
    BIND_ENUM_CONSTANT(DEBUG_DRAW_OVERDRAW);
    BIND_ENUM_CONSTANT(DEBUG_DRAW_WIREFRAME);

    BIND_ENUM_CONSTANT(MSAA_DISABLED);
    BIND_ENUM_CONSTANT(MSAA_2X);
    BIND_ENUM_CONSTANT(MSAA_4X);
    BIND_ENUM_CONSTANT(MSAA_8X);
    BIND_ENUM_CONSTANT(MSAA_16X);

    BIND_ENUM_CONSTANT(USAGE_2D);
    BIND_ENUM_CONSTANT(USAGE_2D_NO_SAMPLING);
    BIND_ENUM_CONSTANT(USAGE_3D);
    BIND_ENUM_CONSTANT(USAGE_3D_NO_EFFECTS);

    BIND_ENUM_CONSTANT(CLEAR_MODE_ALWAYS);
    BIND_ENUM_CONSTANT(CLEAR_MODE_NEVER);
    BIND_ENUM_CONSTANT(CLEAR_MODE_ONLY_NEXT_FRAME);
}

void Viewport::_subwindow_visibility_changed() {

    // unfortunately, we don't know the sender, i.e. which subwindow changed;
    // so we have to check them all.
    gui.subwindow_visibility_dirty = true;
}

Viewport::Viewport() {

    world_2d = make_ref_counted<World2D>();

    viewport = RenderingServer::get_singleton()->viewport_create();
    texture_rid = RenderingServer::get_singleton()->viewport_get_texture(viewport);
    texture_flags = 0;

    render_direct_to_screen = false;

    default_texture = make_ref_counted<ViewportTexture>();
    default_texture->vp = const_cast<Viewport *>(this);
    viewport_textures.insert(default_texture.get());
    RenderingServer::get_singleton()->texture_set_proxy(default_texture->proxy, texture_rid);

    //internal_listener = SpatialSoundServer::get_singleton()->listener_create();
    audio_listener = false;
    //internal_listener_2d = SpatialSound2DServer::get_singleton()->listener_create();
    audio_listener_2d = false;
    transparent_bg = false;
    parent = nullptr;
    listener = nullptr;
    camera = nullptr;
    override_canvas_transform = false;
    canvas_layers.insert(nullptr); // This eases picking code (interpreted as the canvas of the Viewport)
    arvr = false;
    size_override = false;
    size_override_stretch = false;
    size_override_size = Size2(1, 1);
    gen_mipmaps = false;

    vflip = false;

    //clear=true;
    update_mode = UPDATE_WHEN_VISIBLE;

    physics_object_picking = false;
    physics_object_capture = 0;
    physics_object_over = 0;
    physics_has_last_mousepos = false;
    physics_last_mousepos = Vector2(Math_INF, Math_INF);

    shadow_atlas_size = 0;
    for (int i = 0; i < 4; i++) {
        shadow_atlas_quadrant_subdiv[i] = SHADOW_ATLAS_QUADRANT_SUBDIV_MAX;
    }
    set_shadow_atlas_quadrant_subdiv(0, SHADOW_ATLAS_QUADRANT_SUBDIV_4);
    set_shadow_atlas_quadrant_subdiv(1, SHADOW_ATLAS_QUADRANT_SUBDIV_4);
    set_shadow_atlas_quadrant_subdiv(2, SHADOW_ATLAS_QUADRANT_SUBDIV_16);
    set_shadow_atlas_quadrant_subdiv(3, SHADOW_ATLAS_QUADRANT_SUBDIV_64);

    String id = itos(get_instance_id());
    input_group = StringName("_vp_input" + id);
    gui_input_group = StringName("_vp_gui_input" + id);
    unhandled_input_group = StringName("_vp_unhandled_input" + id);
    unhandled_key_input_group = StringName("_vp_unhandled_key_input" + id);

    disable_input = false;
    disable_3d = false;
    keep_3d_linear = false;

    //window tooltip
    gui.tooltip_timer = -1;

    //gui.tooltip_timer->force_parent_owned();
    gui.tooltip_delay = GLOBAL_DEF("gui/timers/tooltip_delay_sec", 0.5);
    ProjectSettings::get_singleton()->set_custom_property_info("gui/timers/tooltip_delay_sec", PropertyInfo(VariantType::FLOAT, "gui/timers/tooltip_delay_sec", PropertyHint::Range, "0,5,0.01,or_greater")); // No negative numbers

    gui.tooltip = nullptr;
    gui.tooltip_label = nullptr;
    gui.drag_preview = nullptr;
    gui.drag_attempted = false;
    gui.canvas_sort_index = 0;
    gui.roots_order_dirty = false;
    gui.mouse_focus = nullptr;
    gui.last_mouse_focus = nullptr;

    msaa = MSAA_DISABLED;
    hdr = true;

    usage = USAGE_3D;
    debug_draw = DEBUG_DRAW_DISABLED;
    clear_mode = CLEAR_MODE_ALWAYS;

    snap_controls_to_pixels = true;
    physics_last_mouse_state.alt = false;
    physics_last_mouse_state.control = false;
    physics_last_mouse_state.shift = false;
    physics_last_mouse_state.meta = false;
    physics_last_mouse_state.mouse_mask = 0;
    local_input_handled = false;
    handle_input_locally = true;
    physics_last_id = 0; //ensures first time there will be a check
}

Viewport::~Viewport() {

    //erase itself from viewport textures
    for (ViewportTexture * E : viewport_textures) {
        E->vp = nullptr;
    }
    RenderingServer::get_singleton()->free_rid(viewport);
    //SpatialSoundServer::get_singleton()->free(internal_listener);
    //SpatialSound2DServer::get_singleton()->free(internal_listener_2d);
}
