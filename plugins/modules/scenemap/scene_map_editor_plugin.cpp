#include "scene_map_editor_plugin.h"

#include "core/object.h"
#include "core/class_db.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/editor_node.h"
#include "editor/plugins/spatial_editor_plugin.h"
#include "scene/gui/item_list.h"
#include "scene/gui/tree.h"

IMPL_GDCLASS(SceneMapEditor)
IMPL_GDCLASS(SceneMapEditorPlugin)


SceneMapEditor::SceneMapEditor(EditorNode* p_editor) {
    int mw = EDITOR_DEF(("editors/scene_map/palette_min_width"), 230);

    VBoxContainer* track_vbox = memnew(VBoxContainer);
    add_child(track_vbox);

    Button* select_all_button = memnew(Button);
    select_all_button->set_text(TTR("Select All/None"));
    track_vbox->add_child(select_all_button);

    auto t = memnew(Tree);
    t->set_h_size_flags(SIZE_EXPAND_FILL);
    t->set_v_size_flags(SIZE_EXPAND_FILL);
    t->set_custom_minimum_size(Size2(100 * EDSCALE, 100 * EDSCALE));
    t->set_hide_root(true);
    t->clear();
    t->set_columns(2);
    t->set_column_title(0,"Name");
    t->set_column_title(1, "Count");
    t->set_column_titles_visible(true);
    TreeItem* troot = t->create_item();
    for(int i=0; i<12; ++i) {
        auto it = t->create_item(troot);

        it->set_text(0, "Omni.bin");
        it->set_text(1, "12");
    }

    track_vbox->add_margin_child(TTR("Library:"), t, true);

    scene_library_palette = memnew(Tree);
    add_child(scene_library_palette);
    scene_library_palette->set_v_size_flags(SIZE_EXPAND_FILL);
    //mesh_library_palette->connect("gui_input", this, "_mesh_library_palette_input");

    info_message = memnew(Label);
    info_message->set_text(TTR("Give a SceneLibrary resource to this SceneMap to use its elements."));
    info_message->set_valign(Label::VALIGN_CENTER);
    info_message->set_align(Label::ALIGN_CENTER);
    info_message->set_autowrap(true);
    info_message->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
    info_message->set_anchors_and_margins_preset(PRESET_WIDE, PRESET_MODE_KEEP_SIZE, 8 * EDSCALE);
    scene_library_palette->add_child(info_message);

}
SceneMapEditor::~SceneMapEditor() {

}
void SceneMapEditor::edit(SceneMap *p_scenemap) {
    m_node = p_scenemap;

    set_process(true);

}

void SceneMapEditor::_notification(int p_what) {
    NodeNotification nn=static_cast<NodeNotification>(p_what);
    switch(p_what) {
    case NOTIFICATION_PROCESS: {
        if (!m_node) {
            return;
        }

        Transform xf = m_node->get_global_transform();

        /*if (xf != grid_xform) {
            for (int i = 0; i < 3; i++) {

                RenderingServer::get_singleton()->instance_set_transform(grid_instance[i], xf * edit_grid_xform);
            }
            grid_xform = xf;
        }*/
        //Ref<SceneLibrary> cgmt = m_node->get_scene_library();
        //if (cgmt.get() != last_mesh_library)
        //    update_palette();

        /*if (lock_view) {

            Plane p;
            p.normal[edit_axis] = 1.0;
            p.d = edit_floor[edit_axis] * m_node->get_cell_size()[edit_axis];
            p = m_node->get_transform().xform(p); // plane to snap

            SpatialEditorPlugin* sep = object_cast<SpatialEditorPlugin>(m_editor->get_editor_plugin_screen());
            if (sep)
                sep->snap_cursor_to_plane(p);
        }*/
    } break;
    default:
        printf("SceneMapEditor disregarding notification:%d\n", p_what);

    }
}

SceneMapEditorPlugin::SceneMapEditorPlugin(EditorNode* editor) {

    EDITOR_DEF(("editors/scene_map/editor_side"), 1);
    EditorSettings::get_singleton()->add_property_hint(PropertyInfo(VariantType::INT, "editors/scene_map/editor_side", PropertyHint::Enum, "Left,Right"));

    scene_map_editor = memnew(SceneMapEditor(editor));
    switch ((int)EditorSettings::get_singleton()->get("editors/scene_map/editor_side")) {
    case 0: { // Left.
        add_control_to_container(CONTAINER_SPATIAL_EDITOR_SIDE_LEFT, scene_map_editor);
    } break;
    case 1: { // Right.
        add_control_to_container(CONTAINER_SPATIAL_EDITOR_SIDE_RIGHT, scene_map_editor);
    } break;
    }
    scene_map_editor->hide();

}

SceneMapEditorPlugin::~SceneMapEditorPlugin() {

}
bool SceneMapEditorPlugin::handles(Object* p_object) const {
    //TODO: consider object_cast instead?
    return p_object->is_class("SceneMap");
}

void SceneMapEditorPlugin::make_visible(bool p_visible) {

    if (p_visible) {
        scene_map_editor->show();
        //scene_map_editor->spatial_editor_hb->show();
        scene_map_editor->set_process(true);
    }
    else {

        //grid_map_editor->spatial_editor_hb->hide();
        scene_map_editor->hide();
        scene_map_editor->edit(nullptr);
        scene_map_editor->set_process(false);
    }
}
