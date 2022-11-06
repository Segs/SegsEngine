/*************************************************************************/
/*  editor_plugin.cpp                                                    */
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

#include "editor_plugin.h"
#include "editor_export.h"


#include <utility>

#include "core/method_bind.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/import/resource_importer_scene.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor_resource_preview.h"
#include "main/main_class.h"
#include "project_settings_editor.h"
#include "plugins/canvas_item_editor_plugin.h"
#include "plugins/node_3d_editor_plugin.h"
#include "scene/3d/camera_3d.h"
#include "scene/gui/popup_menu.h"
#include "servers/rendering_server.h"
#include "filesystem_dock.h"

IMPL_GDCLASS(EditorInterface)
IMPL_GDCLASS(EditorPlugin)

VARIANT_ENUM_CAST(EditorPlugin::CustomControlContainer);
VARIANT_ENUM_CAST(EditorPlugin::DockSlot);

Array EditorInterface::_make_mesh_previews(const Array &p_meshes, int p_preview_size) {

    Vector<Ref<Mesh> > meshes;

    for (int i = 0; i < p_meshes.size(); i++) {
        meshes.push_back(refFromVariant<Mesh>(p_meshes[i]));
    }

    Vector<Ref<Texture> > textures = make_mesh_previews(meshes, nullptr, p_preview_size);
    Array ret;
    for (auto & texture : textures) {
        ret.emplace_back(texture);
    }

    return ret;
}

Vector<Ref<Texture>> EditorInterface::make_mesh_previews(const Vector<Ref<Mesh>> &p_meshes,
        Vector<Transform> *p_transforms, int p_preview_size) {

    int size = p_preview_size;

    RenderingEntity scenario = RenderingServer::get_singleton()->scenario_create();

    RenderingEntity viewport = RenderingServer::get_singleton()->viewport_create();
    RenderingServer::get_singleton()->viewport_set_update_mode(viewport, RS::VIEWPORT_UPDATE_ALWAYS);
    RenderingServer::get_singleton()->viewport_set_vflip(viewport, true);
    RenderingServer::get_singleton()->viewport_set_scenario(viewport, scenario);
    RenderingServer::get_singleton()->viewport_set_size(viewport, size, size);
    RenderingServer::get_singleton()->viewport_set_transparent_background(viewport, true);
    RenderingServer::get_singleton()->viewport_set_active(viewport, true);
    RenderingEntity viewport_texture = RenderingServer::get_singleton()->viewport_get_texture(viewport);

    RenderingEntity camera = RenderingServer::get_singleton()->camera_create();
    RenderingServer::get_singleton()->viewport_attach_camera(viewport, camera);

    RenderingEntity light = RenderingServer::get_singleton()->directional_light_create();
    RenderingEntity light_instance = RenderingServer::get_singleton()->instance_create2(light, scenario);

    RenderingEntity light2 = RenderingServer::get_singleton()->directional_light_create();
    RenderingServer::get_singleton()->light_set_color(light2, Color(0.7, 0.7, 0.7));
    RenderingEntity light_instance2 = RenderingServer::get_singleton()->instance_create2(light2, scenario);

    EditorProgress ep(("mlib"), TTR("Creating Mesh Previews"), p_meshes.size());

    Vector<Ref<Texture> > textures;

    for (int i = 0; i < p_meshes.size(); i++) {

        Ref<Mesh> mesh = p_meshes[i];
        if (not mesh) {
            textures.push_back(Ref<Texture>());
            continue;
        }

        Transform mesh_xform;
        if (p_transforms != nullptr) {
            mesh_xform = (*p_transforms)[i];
        }

        RenderingEntity inst = RenderingServer::get_singleton()->instance_create2(mesh->get_rid(), scenario);
        RenderingServer::get_singleton()->instance_set_transform(inst, mesh_xform);

        AABB aabb = mesh->get_aabb();
        Vector3 ofs = aabb.position + aabb.size * 0.5f;
        aabb.position -= ofs;
        Transform xform;
        xform.basis = Basis().rotated(Vector3(0, 1, 0), -Math_PI / 6);
        xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 6) * xform.basis;
        AABB rot_aabb = xform.xform(aabb);
        float m = M_MAX(rot_aabb.size.x, rot_aabb.size.y) * 0.5f;
        if (m == 0.0f) {
            textures.push_back(Ref<Texture>());
            continue;
        }
        xform.origin = -xform.basis.xform(ofs); //-ofs*m;
        xform.origin.z -= rot_aabb.size.z * 2;
        xform.invert();
        xform = mesh_xform * xform;

        RenderingServer::get_singleton()->camera_set_transform(camera, xform * Transform(Basis(), Vector3(0, 0, 3)));
        RenderingServer::get_singleton()->camera_set_orthogonal(camera, m * 2, 0.01f, 1000.0f);

        RenderingServer::get_singleton()->instance_set_transform(light_instance, xform * Transform().looking_at(Vector3(-2, -1, -1), Vector3(0, 1, 0)));
        RenderingServer::get_singleton()->instance_set_transform(light_instance2, xform * Transform().looking_at(Vector3(+1, -1, -2), Vector3(0, 1, 0)));

        ep.step(TTR("Thumbnail..."), i);
        Main::iteration();
        Main::iteration();
        Ref<Image> img = RenderingServer::get_singleton()->texture_get_data(viewport_texture);
        ERR_CONTINUE(not img || img->is_empty());
        Ref<ImageTexture> it(make_ref_counted<ImageTexture>());
        it->create_from_image(img);

        RenderingServer::get_singleton()->free_rid(inst);

        textures.push_back(it);
    }

    RenderingServer::get_singleton()->free_rid(viewport);
    RenderingServer::get_singleton()->free_rid(light);
    RenderingServer::get_singleton()->free_rid(light_instance);
    RenderingServer::get_singleton()->free_rid(light2);
    RenderingServer::get_singleton()->free_rid(light_instance2);
    RenderingServer::get_singleton()->free_rid(camera);
    RenderingServer::get_singleton()->free_rid(scenario);

    return textures;
}

void EditorInterface::set_main_screen_editor(const StringName &p_name) {
    EditorNode::get_singleton()->select_editor_by_name(p_name);
}

Control *EditorInterface::get_editor_viewport() {

    return EditorNode::get_singleton()->get_viewport();
}

void EditorInterface::edit_resource(const Ref<Resource> &p_resource) {

    EditorNode::get_singleton()->edit_resource(p_resource);
}

void EditorInterface::edit_node(Node *p_node) {
    EditorNode::get_singleton()->edit_node(p_node);
}

void EditorInterface::edit_script(const Ref<Script> &p_script, int p_line, int p_col, bool p_grab_focus) {
    ScriptEditor::get_singleton()->edit(p_script, p_line, p_col, p_grab_focus);
}
void EditorInterface::open_scene_from_path(StringView scene_path) {

    if (EditorNode::get_singleton()->is_changing_scene()) {
        return;
    }

    EditorNode::get_singleton()->open_request(scene_path);
}

void EditorInterface::reload_scene_from_path(StringView scene_path) {

    if (EditorNode::get_singleton()->is_changing_scene()) {
        return;
    }

    EditorNode::get_singleton()->reload_scene(scene_path);
}

void EditorInterface::play_main_scene() {
    EditorNode::get_singleton()->run_play();
}

void EditorInterface::play_current_scene() {
    EditorNode::get_singleton()->run_play_current();
}

void EditorInterface::play_custom_scene(const String &scene_path) {
    EditorNode::get_singleton()->run_play_custom(scene_path);
}

void EditorInterface::stop_playing_scene() {
    EditorNode::get_singleton()->run_stop();
}

bool EditorInterface::is_playing_scene() const {
    return EditorNode::get_singleton()->is_run_playing();
}

String EditorInterface::get_playing_scene() const {
    return EditorNode::get_singleton()->get_run_playing_scene();
}

Node *EditorInterface::get_edited_scene_root() {
    return EditorNode::get_singleton()->get_edited_scene();
}

Array EditorInterface::get_open_scenes() const {

    Array ret;
    Vector<EditorData::EditedScene> scenes = EditorNode::get_editor_data().get_edited_scenes();

    int scns_amount = scenes.size();
    for (int idx_scn = 0; idx_scn < scns_amount; idx_scn++) {
        if (scenes[idx_scn].root == nullptr)
            continue;
        ret.push_back(scenes[idx_scn].root->get_filename());
    }
    return ret;
}

ScriptEditor *EditorInterface::get_script_editor() {
    return ScriptEditor::get_singleton();
}

void EditorInterface::select_file(StringView p_file) {
    EditorNode::get_singleton()->get_filesystem_dock()->select_file(p_file);
}

String EditorInterface::get_selected_path() const {
    return EditorNode::get_singleton()->get_filesystem_dock()->get_selected_path();
}

const String &EditorInterface::get_current_path() const {
    return EditorNode::get_singleton()->get_filesystem_dock()->get_current_path();
}

void EditorInterface::inspect_object(Object *p_obj, StringView p_for_property, bool p_inspector_only) {

    EditorNode::get_singleton()->push_item(p_obj, p_for_property, p_inspector_only);
}

EditorFileSystem *EditorInterface::get_resource_file_system() {
    return EditorFileSystem::get_singleton();
}

FileSystemDock *EditorInterface::get_file_system_dock() {
    return EditorNode::get_singleton()->get_filesystem_dock();
}

EditorSelection *EditorInterface::get_selection() {
    return EditorNode::get_singleton()->get_editor_selection();
}

Ref<EditorSettings> EditorInterface::get_editor_settings() {
    return Ref<EditorSettings>(EditorSettings::get_singleton());
}

EditorResourcePreview *EditorInterface::get_resource_previewer() {
    return EditorResourcePreview::get_singleton();
}

Control *EditorInterface::get_base_control() {

    return EditorNode::get_singleton()->get_gui_base();
}

float EditorInterface::get_editor_scale() const {
    return EDSCALE;
}

void EditorInterface::set_plugin_enabled(StringView p_plugin, bool p_enabled) {
    EditorNode::get_singleton()->set_addon_plugin_enabled(p_plugin, p_enabled, true);
}

bool EditorInterface::is_plugin_enabled(const StringName &p_plugin) const {
    return EditorNode::get_singleton()->is_addon_plugin_enabled(p_plugin);
}

EditorInspector *EditorInterface::get_inspector() const {
    return EditorNode::get_singleton()->get_inspector();
}

Error EditorInterface::save_scene() {
    if (!get_edited_scene_root())
        return ERR_CANT_CREATE;
    if (get_edited_scene_root()->get_filename().empty())
        return ERR_CANT_CREATE;

    save_scene_as(get_edited_scene_root()->get_filename());
    return OK;
}

void EditorInterface::save_scene_as(StringView p_scene, bool p_with_preview) {

    EditorNode::get_singleton()->save_scene_to_path(p_scene, p_with_preview);
}

void EditorInterface::set_distraction_free_mode(bool p_enter) {
    EditorNode::get_singleton()->set_distraction_free_mode(p_enter);
}

EditorInterface *EditorInterface::singleton = nullptr;

void EditorInterface::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("inspect_object", {"object", "for_property", "inspector_only"}), &EditorInterface::inspect_object, {DEFVAL(StringView("")),DEFVAL(false)});
    SE_BIND_METHOD(EditorInterface,get_selection);
    SE_BIND_METHOD(EditorInterface,get_editor_settings);
    SE_BIND_METHOD(EditorInterface,get_script_editor);
    SE_BIND_METHOD(EditorInterface,get_base_control);
    SE_BIND_METHOD(EditorInterface,get_editor_scale);
    SE_BIND_METHOD(EditorInterface,edit_resource);
    SE_BIND_METHOD(EditorInterface,edit_node);
    MethodBinder::bind_method(D_METHOD("edit_script", {"script", "line", "column", "grab_focus"}), &EditorInterface::edit_script, {DEFVAL(-1), DEFVAL(0), DEFVAL(true)});
    SE_BIND_METHOD(EditorInterface,open_scene_from_path);
    SE_BIND_METHOD(EditorInterface,reload_scene_from_path);
    SE_BIND_METHOD(EditorInterface,play_main_scene);
    SE_BIND_METHOD(EditorInterface,play_current_scene);
    SE_BIND_METHOD(EditorInterface,play_custom_scene);
    SE_BIND_METHOD(EditorInterface,stop_playing_scene);
    SE_BIND_METHOD(EditorInterface,is_playing_scene);
    SE_BIND_METHOD(EditorInterface,get_playing_scene);
    SE_BIND_METHOD(EditorInterface,get_open_scenes);
    SE_BIND_METHOD(EditorInterface,get_edited_scene_root);
    SE_BIND_METHOD(EditorInterface,get_resource_previewer);
    SE_BIND_METHOD(EditorInterface,get_resource_file_system);
    SE_BIND_METHOD(EditorInterface,get_editor_viewport);
    MethodBinder::bind_method(D_METHOD("make_mesh_previews", {"meshes", "preview_size"}), &EditorInterface::_make_mesh_previews);
    SE_BIND_METHOD(EditorInterface,select_file);
    SE_BIND_METHOD(EditorInterface,get_selected_path);
    SE_BIND_METHOD(EditorInterface,get_current_path);
    SE_BIND_METHOD(EditorInterface,get_file_system_dock);

    SE_BIND_METHOD(EditorInterface,set_plugin_enabled);
    SE_BIND_METHOD(EditorInterface,is_plugin_enabled);

    SE_BIND_METHOD(EditorInterface,get_inspector);

    SE_BIND_METHOD(EditorInterface,save_scene);
    MethodBinder::bind_method(D_METHOD("save_scene_as", {"path", "with_preview"}), &EditorInterface::save_scene_as, {DEFVAL(true)});

    SE_BIND_METHOD(EditorInterface,set_main_screen_editor);
    SE_BIND_METHOD(EditorInterface,set_distraction_free_mode);
}

EditorInterface::EditorInterface() {
    singleton = this;
}

///////////////////////////////////////////
void EditorPlugin::add_custom_type(const StringName &p_type, const StringName &p_base, const Ref<Script> &p_script, const Ref<Texture> &p_icon) {

    EditorNode::get_editor_data().add_custom_type(p_type, p_base, p_script, p_icon);
}

void EditorPlugin::remove_custom_type(const StringName &p_type) {

    EditorNode::get_editor_data().remove_custom_type(p_type);
}

void EditorPlugin::add_autoload_singleton(const StringName &p_name, StringView p_path) {
    EditorNode::get_singleton()->get_project_settings()->get_autoload_settings()->autoload_add(p_name, p_path);
}

void EditorPlugin::remove_autoload_singleton(const StringName &p_name) {
    EditorNode::get_singleton()->get_project_settings()->get_autoload_settings()->autoload_remove(p_name);
}

ToolButton *EditorPlugin::add_control_to_bottom_panel(Control *p_control, const StringName &p_title) {
    ERR_FAIL_NULL_V(p_control, nullptr);
    return EditorNode::get_singleton()->add_bottom_panel_item(p_title, p_control);
}

void EditorPlugin::add_control_to_dock(DockSlot p_slot, Control *p_control) {

    ERR_FAIL_NULL(p_control);
    EditorNode::get_singleton()->add_control_to_dock(EditorNode::DockSlot(p_slot), p_control);
}

void EditorPlugin::remove_control_from_docks(Control *p_control) {

    ERR_FAIL_NULL(p_control);
    EditorNode::get_singleton()->remove_control_from_dock(p_control);
}

void EditorPlugin::remove_control_from_bottom_panel(Control *p_control) {

    ERR_FAIL_NULL(p_control);
    EditorNode::get_singleton()->remove_bottom_panel_item(p_control);
}

void EditorPlugin::add_control_to_container(CustomControlContainer p_location, Control *p_control) {
    ERR_FAIL_NULL(p_control);

    switch (p_location) {

        case CONTAINER_TOOLBAR: {

            EditorNode::get_menu_hb()->add_child(p_control);
        } break;

        case CONTAINER_SPATIAL_EDITOR_MENU: {

            Node3DEditor::get_singleton()->add_control_to_menu_panel(p_control);

        } break;
        case CONTAINER_SPATIAL_EDITOR_SIDE_LEFT: {

            Node3DEditor::get_singleton()->add_control_to_left_panel(p_control);

        } break;
        case CONTAINER_SPATIAL_EDITOR_SIDE_RIGHT: {

            Node3DEditor::get_singleton()->add_control_to_right_panel(p_control);

        } break;
        case CONTAINER_SPATIAL_EDITOR_BOTTOM: {

            Node3DEditor::get_singleton()->get_shader_split()->add_child(p_control);

        } break;
        case CONTAINER_CANVAS_EDITOR_MENU: {

            CanvasItemEditor::get_singleton()->add_control_to_menu_panel(p_control);

        } break;
        case CONTAINER_CANVAS_EDITOR_SIDE_LEFT: {

            CanvasItemEditor::get_singleton()->add_control_to_left_panel(p_control);

        } break;
        case CONTAINER_CANVAS_EDITOR_SIDE_RIGHT: {

            CanvasItemEditor::get_singleton()->add_control_to_right_panel(p_control);

        } break;
        case CONTAINER_CANVAS_EDITOR_BOTTOM: {

            CanvasItemEditor::get_singleton()->get_bottom_split()->add_child(p_control);

        } break;
        case CONTAINER_PROPERTY_EDITOR_BOTTOM: {

            EditorNode::get_singleton()->get_inspector_dock_addon_area()->add_child(p_control);

        } break;
        case CONTAINER_PROJECT_SETTING_TAB_LEFT: {

            ProjectSettingsEditor::get_singleton()->get_tabs()->add_child(p_control);
            ProjectSettingsEditor::get_singleton()->get_tabs()->move_child(p_control, 0);

        } break;
        case CONTAINER_PROJECT_SETTING_TAB_RIGHT: {

            ProjectSettingsEditor::get_singleton()->get_tabs()->add_child(p_control);
            ProjectSettingsEditor::get_singleton()->get_tabs()->move_child(p_control, 1);

        } break;
    }
}

void EditorPlugin::remove_control_from_container(CustomControlContainer p_location, Control *p_control) {
    ERR_FAIL_NULL(p_control);

    switch (p_location) {

        case CONTAINER_TOOLBAR: {

            EditorNode::get_menu_hb()->remove_child(p_control);
        } break;

        case CONTAINER_SPATIAL_EDITOR_MENU: {

            Node3DEditor::get_singleton()->remove_control_from_menu_panel(p_control);

        } break;
        case CONTAINER_SPATIAL_EDITOR_SIDE_LEFT: {
            Node3DEditor::get_singleton()->remove_control_from_left_panel(p_control);
        } break;
        case CONTAINER_SPATIAL_EDITOR_SIDE_RIGHT: {

            Node3DEditor::get_singleton()->remove_control_from_right_panel(p_control);

        } break;
        case CONTAINER_SPATIAL_EDITOR_BOTTOM: {

            Node3DEditor::get_singleton()->get_shader_split()->remove_child(p_control);

        } break;
        case CONTAINER_CANVAS_EDITOR_MENU: {

            CanvasItemEditor::get_singleton()->remove_control_from_menu_panel(p_control);

        } break;
        case CONTAINER_CANVAS_EDITOR_SIDE_LEFT: {
            CanvasItemEditor::get_singleton()->remove_control_from_left_panel(p_control);
        } break;
        case CONTAINER_CANVAS_EDITOR_SIDE_RIGHT: {

            CanvasItemEditor::get_singleton()->remove_control_from_right_panel(p_control);

        } break;
        case CONTAINER_CANVAS_EDITOR_BOTTOM: {

            CanvasItemEditor::get_singleton()->get_bottom_split()->remove_child(p_control);

        } break;
        case CONTAINER_PROPERTY_EDITOR_BOTTOM: {

            EditorNode::get_singleton()->get_inspector_dock_addon_area()->remove_child(p_control);

        } break;
        case CONTAINER_PROJECT_SETTING_TAB_LEFT:
        case CONTAINER_PROJECT_SETTING_TAB_RIGHT: {

            ProjectSettingsEditor::get_singleton()->get_tabs()->remove_child(p_control);

        } break;
    }
}

void EditorPlugin::add_tool_menu_item(const StringName &p_name, Object *p_handler, StringView p_callback, const Variant &p_ud) {
    EditorNode::get_singleton()->add_tool_menu_item(p_name, p_handler, p_callback, p_ud);
}

void EditorPlugin::add_tool_submenu_item(const StringName &p_name, Object *p_submenu) {
    ERR_FAIL_NULL(p_submenu);
    PopupMenu *submenu = object_cast<PopupMenu>(p_submenu);
    ERR_FAIL_NULL(submenu);
    EditorNode::get_singleton()->add_tool_submenu_item(p_name, submenu);
}

void EditorPlugin::remove_tool_menu_item(const StringName &p_name) {
    EditorNode::get_singleton()->remove_tool_menu_item(p_name);
}

void EditorPlugin::set_input_event_forwarding_always_enabled() {
    input_event_forwarding_always_enabled = true;
    EditorPluginList *always_input_forwarding_list = EditorNode::get_singleton()->get_editor_plugins_force_input_forwarding();
    always_input_forwarding_list->add_plugin(this);
}

void EditorPlugin::set_force_draw_over_forwarding_enabled() {
    force_draw_over_forwarding_enabled = true;
    EditorPluginList *always_draw_over_forwarding_list = EditorNode::get_singleton()->get_editor_plugins_force_over();
    always_draw_over_forwarding_list->add_plugin(this);
}

void EditorPlugin::notify_scene_changed(const Node *scn_root) {
    emit_signal("scene_changed", Variant(scn_root));
}

void EditorPlugin::notify_main_screen_changed(StringView screen_name) {

    if (last_main_screen_name == screen_name)
        return;

    emit_signal("main_screen_changed", screen_name);
    last_main_screen_name = screen_name;
}

void EditorPlugin::notify_scene_closed(StringView scene_filepath) {
    emit_signal("scene_closed", scene_filepath);
}

void EditorPlugin::notify_resource_saved(const Ref<Resource> &p_resource) {
    emit_signal("resource_saved", p_resource);
}

bool EditorPlugin::forward_canvas_gui_input(const Ref<InputEvent> &p_event) {

    if (get_script_instance() && get_script_instance()->has_method("forward_canvas_gui_input")) {
        return get_script_instance()->call("forward_canvas_gui_input", p_event).as<bool>();
    }
    return false;
}

void EditorPlugin::forward_canvas_draw_over_viewport(Control *p_overlay) {

    if (get_script_instance() && get_script_instance()->has_method("forward_canvas_draw_over_viewport")) {
        get_script_instance()->call("forward_canvas_draw_over_viewport", Variant(p_overlay));
    }
}

void EditorPlugin::forward_canvas_force_draw_over_viewport(Control *p_overlay) {

    if (get_script_instance() && get_script_instance()->has_method("forward_canvas_force_draw_over_viewport")) {
        get_script_instance()->call("forward_canvas_force_draw_over_viewport", Variant(p_overlay));
    }
}

// Updates the overlays of the 2D viewport or, if in 3D mode, of every 3D viewport.
int EditorPlugin::update_overlays() const {

    if (Node3DEditor::get_singleton()->is_visible()) {
        int count = 0;
        for (uint32_t i = 0; i < Node3DEditor::VIEWPORTS_COUNT; i++) {
            Node3DEditorViewport *vp = Node3DEditor::get_singleton()->get_editor_viewport(i);
            if (vp->is_visible()) {
                vp->update_surface();
                count++;
            }
        }
        return count;
    } else {
        // This will update the normal viewport itself as well
        CanvasItemEditor::get_singleton()->get_viewport_control()->update();
        return 1;
    }
}

bool EditorPlugin::forward_spatial_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {

    if (get_script_instance() && get_script_instance()->has_method("forward_spatial_gui_input")) {
        return get_script_instance()->call("forward_spatial_gui_input", Variant(p_camera), p_event).as<bool>();
    }

    return false;
}

void EditorPlugin::forward_spatial_draw_over_viewport(Control *p_overlay) {

    if (get_script_instance() && get_script_instance()->has_method("forward_spatial_draw_over_viewport")) {
        get_script_instance()->call("forward_spatial_draw_over_viewport", Variant(p_overlay));
    }
}

void EditorPlugin::forward_spatial_force_draw_over_viewport(Control *p_overlay) {

    if (get_script_instance() && get_script_instance()->has_method("forward_spatial_force_draw_over_viewport")) {
        get_script_instance()->call("forward_spatial_force_draw_over_viewport", Variant(p_overlay));
    }
}
StringView EditorPlugin::get_name() const {
    thread_local char namebuf[512];
    if (get_script_instance() && get_script_instance()->has_method("get_plugin_name")) {
        namebuf[0]=0;
        strncpy(namebuf,get_script_instance()->call("get_plugin_name").as<String>().c_str(),511);
        return namebuf;
    }

    return StringView();
}
Ref<Texture> EditorPlugin::get_icon() const {

    if (get_script_instance() && get_script_instance()->has_method("get_plugin_icon")) {
        return refFromVariant<Texture>(get_script_instance()->call("get_plugin_icon"));
    }

    return Ref<Texture>();
}
bool EditorPlugin::has_main_screen() const {

    if (get_script_instance() && get_script_instance()->has_method("has_main_screen")) {
        return get_script_instance()->call("has_main_screen").as<bool>();
    }

    return false;
}
void EditorPlugin::make_visible(bool p_visible) {

    if (get_script_instance() && get_script_instance()->has_method("make_visible")) {
        get_script_instance()->call("make_visible", p_visible);
    }
}

void EditorPlugin::edit(Object *p_object) {

    if (get_script_instance() && get_script_instance()->has_method("edit")) {
        if (p_object->is_class("Resource")) {
            get_script_instance()->call("edit", Ref<Resource>(object_cast<Resource>(p_object)));
        } else {
            get_script_instance()->call("edit", Variant(p_object));
        }
    }
}

bool EditorPlugin::handles(Object *p_object) const {

    if (get_script_instance() && get_script_instance()->has_method("handles")) {
        return get_script_instance()->call("handles", Variant(p_object)).as<bool>();
    }

    return false;
}
Dictionary EditorPlugin::get_state() const {

    if (get_script_instance() && get_script_instance()->has_method("get_state")) {
        return get_script_instance()->call("get_state").as<Dictionary>();
    }

    return Dictionary();
}

void EditorPlugin::set_state(const Dictionary &p_state) {

    if (get_script_instance() && get_script_instance()->has_method("set_state")) {
        get_script_instance()->call("set_state", p_state);
    }
}

void EditorPlugin::clear() {

    if (get_script_instance() && get_script_instance()->has_method("clear")) {
        get_script_instance()->call("clear");
    }
}

// if editor references external resources/scenes, save them
void EditorPlugin::save_external_data() {

    if (get_script_instance() && get_script_instance()->has_method("save_external_data")) {
        get_script_instance()->call("save_external_data");
    }
}

// if changes are pending in editor, apply them
void EditorPlugin::apply_changes() {

    if (get_script_instance() && get_script_instance()->has_method("apply_changes")) {
        get_script_instance()->call("apply_changes");
    }
}

void EditorPlugin::get_breakpoints(Vector<String> *p_breakpoints) {

    if (get_script_instance() && get_script_instance()->has_method("get_breakpoints")) {
        PoolVector<String> arr(get_script_instance()->call("get_breakpoints").as<PoolVector<String>>());
        for (int i = 0; i < arr.size(); i++)
            p_breakpoints->push_back(arr[i]);
    }
}
bool EditorPlugin::get_remove_list(Vector<Node *> *p_list) {

    return false;
}

void EditorPlugin::restore_global_state() {}
void EditorPlugin::save_global_state() {}

void EditorPlugin::add_import_plugin(const Ref<EditorImportPlugin> &p_importer) {
    ERR_FAIL_COND(!p_importer);
    ResourceFormatImporter::get_singleton()->add_importer(p_importer);
    EditorFileSystem::get_singleton()->call_deferred([] {EditorFileSystem::get_singleton()->scan(); });
}

void EditorPlugin::remove_import_plugin(const Ref<EditorImportPlugin> &p_importer) {
    ERR_FAIL_COND(!p_importer);
    ResourceFormatImporter::get_singleton()->remove_importer(p_importer);
    EditorFileSystem::get_singleton()->call_deferred([] {EditorFileSystem::get_singleton()->scan(); });
}

void EditorPlugin::add_export_plugin(const Ref<EditorExportPlugin> &p_exporter) {
    ERR_FAIL_COND(!p_exporter);
    EditorExport::get_singleton()->add_export_plugin(p_exporter);
}

void EditorPlugin::remove_export_plugin(const Ref<EditorExportPlugin> &p_exporter) {
    ERR_FAIL_COND(!p_exporter);
    EditorExport::get_singleton()->remove_export_plugin(p_exporter);
}

void EditorPlugin::add_spatial_gizmo_plugin(const Ref<EditorSpatialGizmoPlugin> &p_gizmo_plugin) {
    ERR_FAIL_COND(!p_gizmo_plugin);
    Node3DEditor::get_singleton()->add_gizmo_plugin(p_gizmo_plugin);
}

void EditorPlugin::remove_spatial_gizmo_plugin(const Ref<EditorSpatialGizmoPlugin> &p_gizmo_plugin) {
    ERR_FAIL_COND(!p_gizmo_plugin);
    Node3DEditor::get_singleton()->remove_gizmo_plugin(p_gizmo_plugin);
}

void EditorPlugin::add_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin) {
    ERR_FAIL_COND(!p_plugin);
    EditorInspector::add_inspector_plugin(p_plugin);
}

void EditorPlugin::remove_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin) {
    ERR_FAIL_COND(!p_plugin);
    EditorInspector::remove_inspector_plugin(p_plugin);
}
struct ImportWrapper : public EditorSceneImporterInterface {
    Ref<EditorSceneImporter> wrapped;
    ImportWrapper(Ref<EditorSceneImporter> w) : wrapped(eastl::move(w)) {}

    // EditorSceneImporterInterface interface
public:
    uint32_t get_import_flags() const override {
        return wrapped->get_import_flags();
    }
    void get_extensions(Vector<String> &p_extensions) const override {
        wrapped->get_extensions(p_extensions);
    }
    Node *import_scene(StringView p_path, uint32_t p_flags, int p_bake_fps, uint32_t p_compress_flags, Vector<String> *r_missing_deps, Error *r_err) override {
        return wrapped->import_scene(p_path,p_flags,p_bake_fps,p_compress_flags,r_missing_deps,r_err);
    }
    Ref<Animation> import_animation(StringView p_path, uint32_t p_flags, int p_bake_fps) override {
        return wrapped->import_animation(p_path,p_flags,p_bake_fps);
    }
};

void EditorPlugin::add_scene_import_plugin(const Ref<EditorSceneImporter> &p_importer) {
    //TODO: resolve issues with Plugin wrapping for script-side importers
    assert(false);
    ResourceImporterScene::get_singleton()->add_importer(new ImportWrapper(p_importer));
}

void EditorPlugin::remove_scene_import_plugin(const Ref<EditorSceneImporter> &p_importer) {
    //TODO: resolve issues with Plugin wrapping for script-side importers
    assert(false);
    ResourceImporterScene::get_singleton()->remove_importer(new ImportWrapper(p_importer));
}

void EditorPlugin::enable_plugin() {
    // Called when the plugin gets enabled in project settings, after it's added to the tree.
    // You can implement it to register autoloads.

    if (get_script_instance() && get_script_instance()->has_method("enable_plugin")) {
        get_script_instance()->call("enable_plugin");
    }
}

void EditorPlugin::disable_plugin() {
    // Last function called when the plugin gets disabled in project settings.
    // Implement it to cleanup things from the project, such as unregister autoloads.

    if (get_script_instance() && get_script_instance()->has_method("disable_plugin")) {
        get_script_instance()->call("disable_plugin");
    }
}

void EditorPlugin::set_window_layout(Ref<ConfigFile> p_layout) {

    if (get_script_instance() && get_script_instance()->has_method("set_window_layout")) {
        get_script_instance()->call("set_window_layout", p_layout);
    }
}

void EditorPlugin::get_window_layout(Ref<ConfigFile> p_layout) {

    if (get_script_instance() && get_script_instance()->has_method("get_window_layout")) {
        get_script_instance()->call("get_window_layout", p_layout);
    }
}

bool EditorPlugin::build() {

    if (get_script_instance() && get_script_instance()->has_method("build")) {
        return get_script_instance()->call("build").as<bool>();
    }

    return true;
}

void EditorPlugin::queue_save_layout() const {

    EditorNode::get_singleton()->save_layout();
}

void EditorPlugin::make_bottom_panel_item_visible(Control *p_item) {

    EditorNode::get_singleton()->make_bottom_panel_item_visible(p_item);
}

void EditorPlugin::hide_bottom_panel() {

    EditorNode::get_singleton()->hide_bottom_panel();
}

EditorInterface *EditorPlugin::get_editor_interface() {
    return EditorInterface::get_singleton();
}

ScriptCreateDialog *EditorPlugin::get_script_create_dialog() {
    return EditorNode::get_singleton()->get_script_create_dialog();
}

void EditorPlugin::_bind_methods() {

    SE_BIND_METHOD(EditorPlugin,add_control_to_container);
    SE_BIND_METHOD(EditorPlugin,add_control_to_bottom_panel);
    SE_BIND_METHOD(EditorPlugin,add_control_to_dock);
    SE_BIND_METHOD(EditorPlugin,remove_control_from_docks);
    SE_BIND_METHOD(EditorPlugin,remove_control_from_bottom_panel);
    SE_BIND_METHOD(EditorPlugin,remove_control_from_container);
    MethodBinder::bind_method(D_METHOD("add_tool_menu_item", {"name", "handler", "callback", "ud"}), &EditorPlugin::add_tool_menu_item, {DEFVAL(Variant())});
    SE_BIND_METHOD(EditorPlugin,add_tool_submenu_item);
    SE_BIND_METHOD(EditorPlugin,remove_tool_menu_item);
    SE_BIND_METHOD(EditorPlugin,add_custom_type);
    SE_BIND_METHOD(EditorPlugin,remove_custom_type);

    SE_BIND_METHOD(EditorPlugin,add_autoload_singleton);
    SE_BIND_METHOD(EditorPlugin,remove_autoload_singleton);

    SE_BIND_METHOD(EditorPlugin,update_overlays);

    SE_BIND_METHOD(EditorPlugin,make_bottom_panel_item_visible);
    SE_BIND_METHOD(EditorPlugin,hide_bottom_panel);

    SE_BIND_METHOD(EditorPlugin,get_undo_redo);
    SE_BIND_METHOD(EditorPlugin,queue_save_layout);
    SE_BIND_METHOD(EditorPlugin,add_import_plugin);
    SE_BIND_METHOD(EditorPlugin,remove_import_plugin);
    SE_BIND_METHOD(EditorPlugin,add_scene_import_plugin);
    SE_BIND_METHOD(EditorPlugin,remove_scene_import_plugin);
    SE_BIND_METHOD(EditorPlugin,add_export_plugin);
    SE_BIND_METHOD(EditorPlugin,remove_export_plugin);
    SE_BIND_METHOD(EditorPlugin,add_spatial_gizmo_plugin);
    SE_BIND_METHOD(EditorPlugin,remove_spatial_gizmo_plugin);
    SE_BIND_METHOD(EditorPlugin,add_inspector_plugin);
    SE_BIND_METHOD(EditorPlugin,remove_inspector_plugin);
    SE_BIND_METHOD(EditorPlugin,set_input_event_forwarding_always_enabled);
    SE_BIND_METHOD(EditorPlugin,set_force_draw_over_forwarding_enabled);

    SE_BIND_METHOD(EditorPlugin,get_editor_interface);
    SE_BIND_METHOD(EditorPlugin,get_script_create_dialog);

    //TODO: make virtual method names match actual virtual method names ex. get_plugin_icon -> get_icon
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::BOOL, "forward_canvas_gui_input", PropertyInfo(VariantType::OBJECT, "event", PropertyHint::ResourceType, "InputEvent")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("forward_canvas_draw_over_viewport", PropertyInfo(VariantType::OBJECT, "overlay", PropertyHint::ResourceType, "Control")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("forward_canvas_force_draw_over_viewport", PropertyInfo(VariantType::OBJECT, "overlay", PropertyHint::ResourceType, "Control")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::BOOL, "forward_spatial_gui_input", PropertyInfo(VariantType::OBJECT, "camera", PropertyHint::ResourceType, "Camera3D"), PropertyInfo(VariantType::OBJECT, "event", PropertyHint::ResourceType, "InputEvent")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("forward_spatial_draw_over_viewport", PropertyInfo(VariantType::OBJECT, "overlay", PropertyHint::ResourceType, "Control")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("forward_spatial_force_draw_over_viewport", PropertyInfo(VariantType::OBJECT, "overlay", PropertyHint::ResourceType, "Control")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::STRING, "get_plugin_name"));
    ClassDB::add_virtual_method(get_class_static_name(),
            MethodInfo(
                    PropertyInfo(VariantType::OBJECT, "icon", PropertyHint::ResourceType, "Texture"), "get_plugin_icon"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::BOOL, "has_main_screen"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("make_visible", PropertyInfo(VariantType::BOOL, "visible")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("edit", PropertyInfo(VariantType::OBJECT, "object")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::BOOL, "handles", PropertyInfo(VariantType::OBJECT, "object")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::DICTIONARY, "get_state"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("set_state", PropertyInfo(VariantType::DICTIONARY, "state")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("clear"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("save_external_data"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("apply_changes"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::POOL_STRING_ARRAY, "get_breakpoints"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("set_window_layout", PropertyInfo(VariantType::OBJECT, "layout", PropertyHint::ResourceType, "ConfigFile")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("get_window_layout", PropertyInfo(VariantType::OBJECT, "layout", PropertyHint::ResourceType, "ConfigFile")));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::BOOL, "build"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("enable_plugin"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo("disable_plugin"));

    ADD_SIGNAL(MethodInfo("scene_changed", PropertyInfo(VariantType::OBJECT, "scene_root", PropertyHint::ResourceType, "Node")));
    ADD_SIGNAL(MethodInfo("scene_closed", PropertyInfo(VariantType::STRING, "filepath")));
    ADD_SIGNAL(MethodInfo("main_screen_changed", PropertyInfo(VariantType::STRING, "screen_name")));
    ADD_SIGNAL(MethodInfo("resource_saved", PropertyInfo(VariantType::OBJECT, "resource", PropertyHint::ResourceType, "Resource")));

    BIND_ENUM_CONSTANT(CONTAINER_TOOLBAR);
    BIND_ENUM_CONSTANT(CONTAINER_SPATIAL_EDITOR_MENU);
    BIND_ENUM_CONSTANT(CONTAINER_SPATIAL_EDITOR_SIDE_LEFT);
    BIND_ENUM_CONSTANT(CONTAINER_SPATIAL_EDITOR_SIDE_RIGHT);
    BIND_ENUM_CONSTANT(CONTAINER_SPATIAL_EDITOR_BOTTOM);
    BIND_ENUM_CONSTANT(CONTAINER_CANVAS_EDITOR_MENU);
    BIND_ENUM_CONSTANT(CONTAINER_CANVAS_EDITOR_SIDE_LEFT);
    BIND_ENUM_CONSTANT(CONTAINER_CANVAS_EDITOR_SIDE_RIGHT);
    BIND_ENUM_CONSTANT(CONTAINER_CANVAS_EDITOR_BOTTOM);
    BIND_ENUM_CONSTANT(CONTAINER_PROPERTY_EDITOR_BOTTOM);
    BIND_ENUM_CONSTANT(CONTAINER_PROJECT_SETTING_TAB_LEFT);
    BIND_ENUM_CONSTANT(CONTAINER_PROJECT_SETTING_TAB_RIGHT);

    BIND_ENUM_CONSTANT(DOCK_SLOT_LEFT_UL);
    BIND_ENUM_CONSTANT(DOCK_SLOT_LEFT_BL);
    BIND_ENUM_CONSTANT(DOCK_SLOT_LEFT_UR);
    BIND_ENUM_CONSTANT(DOCK_SLOT_LEFT_BR);
    BIND_ENUM_CONSTANT(DOCK_SLOT_RIGHT_UL);
    BIND_ENUM_CONSTANT(DOCK_SLOT_RIGHT_BL);
    BIND_ENUM_CONSTANT(DOCK_SLOT_RIGHT_UR);
    BIND_ENUM_CONSTANT(DOCK_SLOT_RIGHT_BR);
    BIND_ENUM_CONSTANT(DOCK_SLOT_MAX);
}

EditorPlugin::EditorPlugin() = default;

EditorPlugin::~EditorPlugin() = default;

EditorPluginCreateFunc EditorPlugins::creation_funcs[MAX_CREATE_FUNCS];

int EditorPlugins::creation_func_count = 0;
