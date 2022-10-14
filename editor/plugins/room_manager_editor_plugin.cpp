#include "room_manager_editor_plugin.h"

#include "editor/node_3d_editor_gizmos.h"
#include "editor/editor_node.h"
#include "scene/gui/tool_button.h"
#include "scene/3d/room_manager.h"
#include "scene/3d/room.h"
#include "scene/3d/portal.h"
#include "scene/3d/cull_instance_component.h"
#include "core/string_formatter.h"
#include "core/callable_method_pointer.h"
#include "core/method_bind.h"

IMPL_GDCLASS(RoomManagerEditorPlugin)
IMPL_GDCLASS(RoomEditorPlugin)
IMPL_GDCLASS(PortalEditorPlugin)


void RoomManagerEditorPlugin::_flip_portals() {
    if (_room_manager) {
        _room_manager->rooms_flip_portals();
    }
}

void RoomManagerEditorPlugin::edit(Object *p_object) {
    RoomManager *s = object_cast<RoomManager>(p_object);
    if (!s) {
        return;
    }

    _room_manager = s;
}

bool RoomManagerEditorPlugin::handles(Object *p_object) const {
    return p_object->is_class("RoomManager");
}

void RoomManagerEditorPlugin::make_visible(bool p_visible) {
    if (p_visible) {
        button_flip_portals->show();
    } else {
        button_flip_portals->hide();
    }

    Node3DEditor::get_singleton()->show_advanced_portal_tools(p_visible);
}

//void RoomManagerEditorPlugin::_bind_methods() {
//    ClassDB::bind_method("_flip_portals", &RoomManagerEditorPlugin::_flip_portals);
//}

RoomManagerEditorPlugin::RoomManagerEditorPlugin(EditorNode *p_node) {
    editor = p_node;

    button_flip_portals = memnew(ToolButton);
    button_flip_portals->set_button_icon(editor->get_gui_base()->get_theme_icon("Portal", "EditorIcons"));
    button_flip_portals->set_text(TTR("Flip Portals"));
    button_flip_portals->hide();
    button_flip_portals->connect("pressed", callable_mp(this, &RoomManagerEditorPlugin::_flip_portals));
    add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, button_flip_portals);

    _room_manager = nullptr;
    auto *node3d_ed=Node3DEditor::get_singleton();
    Ref<RoomGizmoPlugin> room_gizmo_plugin = Ref<RoomGizmoPlugin>(memnew(RoomGizmoPlugin));
    node3d_ed->add_gizmo_plugin(room_gizmo_plugin);

    Ref<PortalGizmoPlugin> portal_gizmo_plugin = Ref<PortalGizmoPlugin>(memnew(PortalGizmoPlugin));
    node3d_ed->add_gizmo_plugin(portal_gizmo_plugin);

    Ref<OccluderGizmoPlugin> occluder_gizmo_plugin = Ref<OccluderGizmoPlugin>(memnew(OccluderGizmoPlugin));
    node3d_ed->add_gizmo_plugin(occluder_gizmo_plugin);
}

RoomManagerEditorPlugin::~RoomManagerEditorPlugin() {
}

///////////////////////

void RoomEditorPlugin::_generate_points() {
    if (!_room) {
        return;
    }
    const Vector<Vector3> &old_pts = _room->get_points();

    // only generate points if none already exist
    if (!_room->_bound_pts.empty()) {
        _room->set_points({});
    }

    PoolVector<Vector3> pts = _room->generate_points();

       // allow the user to undo generating points, because it is
       // frustrating to lose old data
    undo_redo->create_action(TTR("Room Generate Points"));
    undo_redo->add_do_property(_room, "points", pts);
    undo_redo->add_undo_property(_room, "points", old_pts);
    undo_redo->commit_action();
}
void RoomEditorPlugin::edit(Object *p_object) {
    Room *s = object_cast<Room>(p_object);
    if (!s) {
        return;
    }

    _room = s;

    if (Node3DEditor::get_singleton()->is_visible() && s->_planes.size()) {
        String string = String(s->get_name()) + " [" + itos(s->_planes.size()) + " planes]";
        Node3DEditor::get_singleton()->set_message(string);
    }
}

bool RoomEditorPlugin::handles(Object *p_object) const {
    return p_object->is_class("Room");
}

void RoomEditorPlugin::make_visible(bool p_visible) {
    if (p_visible) {
        button_generate->show();
    } else {
        button_generate->hide();
    }
}

//void RoomEditorPlugin::_bind_methods() {
//    ClassDB::bind_method("_generate_points", &RoomEditorPlugin::_generate_points);
//}

RoomEditorPlugin::RoomEditorPlugin(EditorNode *p_node) {
    editor = p_node;

    button_generate = memnew(ToolButton);
    button_generate->set_button_icon(editor->get_gui_base()->get_theme_icon("Room", "EditorIcons"));
    button_generate->set_text(TTR("Generate Points"));
    button_generate->hide();
    button_generate->connect("pressed", callable_mp(this, &RoomEditorPlugin::_generate_points));
    add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, button_generate);

    _room = nullptr;

    undo_redo = EditorNode::get_undo_redo();
}

RoomEditorPlugin::~RoomEditorPlugin() {
}

///////////////////////

void PortalEditorPlugin::_flip_portal() {
    if (_portal) {
        _portal->flip();
        _portal->_changed();
    }
}

void PortalEditorPlugin::edit(Object *p_object) {
    Portal *p = object_cast<Portal>(p_object);
    if (!p) {
        return;
    }

    _portal = p;
}

bool PortalEditorPlugin::handles(Object *p_object) const {
    return p_object->is_class("Portal");
}

void PortalEditorPlugin::make_visible(bool p_visible) {
    if (p_visible) {
        button_flip->show();
    } else {
        button_flip->hide();
    }
}

//void PortalEditorPlugin::_bind_methods() {
//    ClassDB::bind_method("_flip_portal", &PortalEditorPlugin::_flip_portal);
//}

PortalEditorPlugin::PortalEditorPlugin(EditorNode *p_node) {
    editor = p_node;

    button_flip = memnew(ToolButton);
    button_flip->set_button_icon(editor->get_gui_base()->get_theme_icon("Portal", "EditorIcons"));
    button_flip->set_text(TTR("Flip Portal"));
    button_flip->hide();
    button_flip->connect("pressed", callable_mp(this, &PortalEditorPlugin::_flip_portal));
    add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, button_flip);

    _portal = nullptr;
}

PortalEditorPlugin::~PortalEditorPlugin() {
}
