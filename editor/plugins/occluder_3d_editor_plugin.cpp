#include "occluder_3d_editor_plugin.h"

#include "scene/3d/occluder.h"
#include "scene/gui/tool_button.h"
#include "scene/resources/occluder_shape.h"
#include "core/callable_method_pointer.h"
#include "core/engine.h"
#include "core/method_bind.h"
#include "editor/editor_node.h"
#include "editor/plugins/node_3d_editor_plugin.h"


IMPL_GDCLASS(OccluderEditorPlugin)


///////////////////////

void OccluderEditorPlugin::_center() {
    //TODO: consider storing _center operation in operations queue, to center after the element enters the tree?
    if (!_occluder || !_occluder->is_inside_tree()) {
        return;
    }
    Ref<OccluderShape> ref = _occluder->get_shape();
    if(!ref) {
        return;
    }
    Node3D *parent = object_cast<Node3D>(_occluder->get_parent());
    if (!parent) {
        return;
    }
    float snap = 0.0f;

    if (Engine::get_singleton()->is_editor_hint()) {
        if (Node3DEditor::get_singleton() && Node3DEditor::get_singleton()->is_snap_enabled()) {
            snap = Node3DEditor::get_singleton()->get_translate_snap();
        }
    }

    Transform old_local_xform = _occluder->get_transform();
    Transform new_local_xform = ref->center_node(_occluder->get_global_transform(), parent->get_global_transform(), snap);
    _occluder->property_list_changed_notify();

    undo_redo->create_action(TTR("Occluder Set Transform"));
    undo_redo->add_do_method(_occluder, "set_transform", new_local_xform);
    undo_redo->add_undo_method(_occluder, "set_transform", old_local_xform);
    undo_redo->commit_action();

    _occluder->update_gizmo();
}

void OccluderEditorPlugin::edit(Object *p_object) {
    Occluder *p = object_cast<Occluder>(p_object);
    if (!p) {
        return;
    }

    _occluder = p;
}

bool OccluderEditorPlugin::handles(Object *p_object) const {
    return p_object->is_class("Occluder");
}

void OccluderEditorPlugin::make_visible(bool p_visible) {
    if (p_visible) {
        button_center->show();
    } else {
        button_center->hide();
    }
}

//void OccluderEditorPlugin::_bind_methods() {
//    ClassDB::bind_method("_center", &OccluderEditorPlugin::_center);
//}

OccluderEditorPlugin::OccluderEditorPlugin(EditorNode *p_node) {
    editor = p_node;

    button_center = memnew(ToolButton);
    button_center->set_button_icon(editor->get_gui_base()->get_theme_icon("EditorPosition", "EditorIcons"));
    button_center->set_text(TTR("Center Node"));
    button_center->hide();
    button_center->connect("pressed", callable_mp(this, &OccluderEditorPlugin::_center));
    add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, button_center);

    undo_redo = EditorNode::get_undo_redo();

    _occluder = nullptr;
}

OccluderEditorPlugin::~OccluderEditorPlugin() {
}
