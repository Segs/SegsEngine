/*************************************************************************/
/*  mesh_library_editor_plugin.cpp                                       */
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

#include "scene_library_editor_plugin.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/resource/resource_manager.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "core/string_formatter.h"
#include "main/main.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/navigation_mesh_instance.h"
#include "scene/3d/physics_body_3d.h"
#include "scene/main/viewport.h"
#include "scene/resources/packed_scene.h"
#include "node_3d_editor_plugin.h"
#include "core/translation_helpers.h"
#include "servers/rendering_server.h"
IMPL_GDCLASS(SceneLibraryEditor)
IMPL_GDCLASS(SceneLibraryEditorPlugin)

void SceneLibraryEditor::edit(const Ref<SceneLibrary> &scene_library) {

    m_scene_library = scene_library;
    if (m_scene_library)
        menu->get_popup()->set_item_disabled(menu->get_popup()->get_item_index(MENU_OPTION_UPDATE_FROM_SCENE),
                !m_scene_library->has_meta("_editor_source_scene"));
}

void SceneLibraryEditor::_menu_confirm() {

    switch (option) {

        case MENU_OPTION_REMOVE_ITEM: {

            m_scene_library->remove_item(to_erase);
        } break;
        case MENU_OPTION_UPDATE_FROM_SCENE: {
            String existing = m_scene_library->get_meta("_editor_source_scene").as<String>();
            ERR_FAIL_COND(existing.empty());
            _import_scene_cbk(existing);

        } break;
        default: {
        }
    }
}

void SceneLibraryEditor::_import_scene(Node *p_scene,const Ref<SceneLibrary> &p_library, bool p_merge) {

    if (!p_merge)
        p_library->clear();

    Map<int, MeshInstance3D *> mesh_instances;
    int id = p_library->find_item_by_name(p_scene->get_name());
    if (id < 0) {

        id = p_library->get_last_unused_item_id();
        p_library->create_item(id);
        p_library->set_item_name(id, p_scene->get_name());
    }
    auto store = make_ref_counted<PackedScene>();
    if(OK!=store->pack(p_scene)) {
        ERR_FAIL_MSG("Cannot pack provided node to PackedScene '" + String(p_scene->get_name()) + "'.");

    }
    p_library->set_item_scene(id,store);
    if (true) {
        //TODO: generate packed scene previews here:
        // use m_preview_target stand-alone viewport + `frame_post_draw` signal from visual server
//        Vector<Ref<PackedScene> > meshes;
//        Vector<Transform> transforms;
//        Vector<int> ids = p_library->get_item_list();
//        for (int i = 0; i < ids.size(); i++) {

//            if (mesh_instances.contains(ids[i])) {

//                meshes.emplace_back(p_library->get_item_scene(ids[i]));
//                transforms.emplace_back(mesh_instances[ids[i]]->get_transform());
//            }
//        }

//        Vector<Ref<Texture> > textures = EditorInterface::get_singleton()->make_mesh_previews(meshes, &transforms, EditorSettings::get_singleton()->get("editors/grid_map/preview_size"));
//        int j = 0;
//        for (int i = 0; i < ids.size(); i++) {

//            if (mesh_instances.contains(ids[i])) {

//                p_library->set_item_preview(ids[i], textures[j]);
//                j++;
//            }
//        }
    }
}

void SceneLibraryEditor::_import_scene_cbk(StringView p_str) {

    Ref<PackedScene> ps = dynamic_ref_cast<PackedScene>(gResourceManager().load(p_str, "PackedScene"));
    ERR_FAIL_COND(not ps);
    Node *scene = ps->instance();

    ERR_FAIL_COND_MSG(!scene, "Cannot create an instance from PackedScene '" + String(p_str) + "'.");

    _import_scene(scene, m_scene_library, option == MENU_OPTION_UPDATE_FROM_SCENE);

    memdelete(scene);
    m_scene_library->set_meta("_editor_source_scene", p_str);
    menu->get_popup()->set_item_disabled(menu->get_popup()->get_item_index(MENU_OPTION_UPDATE_FROM_SCENE), false);
}

Error SceneLibraryEditor::update_library_file(Node *p_base_scene, const Ref<SceneLibrary>& ml, bool p_merge) {

    _import_scene(p_base_scene, ml, p_merge);
    return OK;
}

void SceneLibraryEditor::_menu_cbk(int p_option) {

    option = p_option;
    switch (p_option) {

        case MENU_OPTION_ADD_ITEM: {

            m_scene_library->create_item(m_scene_library->get_last_unused_item_id());
        } break;
        case MENU_OPTION_REMOVE_ITEM: {

            String p(editor->get_inspector()->get_selected_path());
            if (StringUtils::begins_with(p,"/SceneLibrary/item") && StringUtils::get_slice_count(p,'/') >= 3) {

                to_erase = StringUtils::to_int(StringUtils::get_slice(p,'/', 3));
                cd->set_text(FormatSN(TTR("Remove item %d?").asCString(), to_erase));
                cd->popup_centered(Size2(300, 60));
            }
        } break;
        case MENU_OPTION_IMPORT_FROM_SCENE: {

            file->popup_centered_ratio();
        } break;
        case MENU_OPTION_UPDATE_FROM_SCENE: {

            cd->set_text(StringName(TTR("Update from existing scene?:\n") + m_scene_library->get_meta("_editor_source_scene").as<String>()));
            cd->popup_centered(Size2(500, 60));
        } break;
    }
}

void SceneLibraryEditor::_bind_methods() {
}

SceneLibraryEditor::SceneLibraryEditor(EditorNode *p_editor) {

    file = memnew(EditorFileDialog);
    file->set_mode(EditorFileDialog::MODE_OPEN_FILE);
    //not for now?
    Vector<String> extensions;
    gResourceManager().get_recognized_extensions_for_type("PackedScene", extensions);
    file->clear_filters();
    file->set_title(TTR("Import Scene"));
    for (const String & ext : extensions) {
        file->add_filter("*." + ext + " ; " + StringUtils::to_upper(ext));
    }
    add_child(file);
    file->connect("file_selected",callable_mp(this, &ClassName::_import_scene_cbk));

    menu = memnew(MenuButton);
    Node3DEditor::get_singleton()->add_control_to_menu_panel(menu);
    menu->set_position(Point2(1, 1));
    menu->set_text(TTR("Scene Library"));
    menu->set_button_icon(EditorNode::get_singleton()->get_gui_base()->get_theme_icon("SceneLibrary", "EditorIcons"));
    menu->get_popup()->add_item(TTR("Add Item"), MENU_OPTION_ADD_ITEM);
    menu->get_popup()->add_item(TTR("Remove Selected Item"), MENU_OPTION_REMOVE_ITEM);
    menu->get_popup()->add_separator();
    menu->get_popup()->add_item(TTR("Import from Scene"), MENU_OPTION_IMPORT_FROM_SCENE);
    menu->get_popup()->add_item(TTR("Update from Scene"), MENU_OPTION_UPDATE_FROM_SCENE);
    menu->get_popup()->set_item_disabled(menu->get_popup()->get_item_index(MENU_OPTION_UPDATE_FROM_SCENE), true);
    menu->get_popup()->connect("id_pressed",callable_mp(this, &ClassName::_menu_cbk));
    menu->hide();

    editor = p_editor;
    cd = memnew(ConfirmationDialog);
    add_child(cd);
    cd->get_ok()->connect("pressed",callable_mp(this, &ClassName::_menu_confirm));
}

void SceneLibraryEditorPlugin::edit(Object *p_node) {

    if (object_cast<SceneLibrary>(p_node)) {
        mesh_library_editor->edit(Ref<SceneLibrary>(object_cast<SceneLibrary>(p_node)));
        mesh_library_editor->show();
    } else
        mesh_library_editor->hide();
}

bool SceneLibraryEditorPlugin::handles(Object *p_node) const {

    return p_node->is_class("SceneLibrary");
}

void SceneLibraryEditorPlugin::make_visible(bool p_visible) {

    if (p_visible) {
        mesh_library_editor->show();
        mesh_library_editor->get_menu_button()->show();
    } else {
        mesh_library_editor->hide();
        mesh_library_editor->get_menu_button()->hide();
    }
}

SceneLibraryEditorPlugin::SceneLibraryEditorPlugin(EditorNode *p_node) {

    EDITOR_DEF("editors/grid_map/preview_size", 64);
    mesh_library_editor = memnew(SceneLibraryEditor(p_node));

    p_node->get_viewport()->add_child(mesh_library_editor);
    mesh_library_editor->set_anchors_and_margins_preset(Control::PRESET_TOP_WIDE);
    mesh_library_editor->set_end(Point2(0, 22));
    mesh_library_editor->hide();
}
