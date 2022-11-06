/*************************************************************************/
/*  resource_importer_scene.h                                            */
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

#pragma once

#include "core/pair.h"
#include "core/io/resource_importer.h"
#include "editor/plugin_interfaces/EditorSceneImporterInterface.h"
#include "scene/resources/animation.h"
#include "scene/resources/mesh.h"
#include "scene/resources/shape.h"

class Material;

class GODOT_EXPORT EditorSceneImporter : public EditorSceneImporterInterface,public RefCounted {

    GDCLASS(EditorSceneImporter,RefCounted)

protected:
    static void _bind_methods();
public:
    Node *import_scene_from_other_importer(StringView p_path, uint32_t p_flags, int p_bake_fps, uint32_t p_compress_flags);
    Ref<Animation> import_animation_from_other_importer(StringView p_path, uint32_t p_flags, int p_bake_fps);

public:
    uint32_t get_import_flags() const override;
    void get_extensions(Vector<String> &r_extensions) const override;
    Node *import_scene(StringView p_path, uint32_t p_flags, int p_bake_fps, uint32_t p_compress_flags, Vector<String> *r_missing_deps, Error *r_err = nullptr) override;
    Ref<Animation> import_animation(StringView p_path, uint32_t p_flags, int p_bake_fps) override;

    EditorSceneImporter() {}
};

class GODOT_EXPORT EditorScenePostImport : public RefCounted {

    GDCLASS(EditorScenePostImport,RefCounted)

    String source_folder;
    String source_file;

protected:
    static void _bind_methods();

public:
    const String &get_source_folder() const;
    const String &get_source_file() const;
    virtual Node *post_import(Node *p_scene);
    virtual void init(StringView p_source_folder, StringView p_source_file);
    EditorScenePostImport();
};

class ResourceImporterScene : public ResourceImporter {
    GDCLASS(ResourceImporterScene,ResourceImporter)

    HashSet<EditorSceneImporterInterface *> scene_importers;

    static ResourceImporterScene *singleton;

    enum Presets {
        PRESET_SEPARATE_MATERIALS,
        PRESET_SEPARATE_MESHES,
        PRESET_SEPARATE_ANIMATIONS,

        PRESET_SINGLE_SCENE,

        PRESET_SEPARATE_MESHES_AND_MATERIALS,
        PRESET_SEPARATE_MESHES_AND_ANIMATIONS,
        PRESET_SEPARATE_MATERIALS_AND_ANIMATIONS,
        PRESET_SEPARATE_MESHES_MATERIALS_AND_ANIMATIONS,

        PRESET_MULTIPLE_SCENES,
        PRESET_MULTIPLE_SCENES_AND_MATERIALS,
        PRESET_MAX
    };

    enum LightBakeMode {
        LIGHT_BAKE_DISABLED,
        LIGHT_BAKE_ENABLE,
        LIGHT_BAKE_LIGHTMAPS
    };

    void _replace_owner(Node *p_node, Node *p_scene, Node *p_new_owner);
    void _add_shapes(Node *p_node, const Vector<Ref<Shape> > &p_shapes);
    Node *_fix_node(Node *p_node, Node *p_root, Map<Ref<Mesh>, Vector<Ref<Shape>>> &collision_map,
            LightBakeMode p_light_bake_mode, Dequeue<Pair<NodePath, Node *>> &r_node_renames);

public:
    static ResourceImporterScene *get_singleton() { return singleton; }

//    const HashSet<EditorSceneImporterInterface *> &get_importers() const { return importers; }

    void add_importer(EditorSceneImporterInterface *p_importer) { scene_importers.insert(p_importer); }
    void remove_importer(EditorSceneImporterInterface *p_importer) { scene_importers.erase(p_importer); }

    const char *get_importer_name() const override;
    const char *get_visible_name() const override;
    void get_recognized_extensions(Vector<String> &p_extensions) const override;
    bool can_import(StringView) const override;
    StringName get_save_extension() const override;
    StringName get_resource_type() const override;


    int get_preset_count() const override;
    StringName get_preset_name(int p_idx) const override;

    void get_import_options(Vector<ImportOption> *r_options, int p_preset = 0) const override;
    bool get_option_visibility(const StringName &p_option, const HashMap<StringName, Variant> &p_options) const override;
    // Import scenes *after* everything else (such as textures).
    int get_import_order() const override { return ResourceImporter::IMPORT_ORDER_SCENE; } //after everything

    void _find_meshes(Node *p_node, Map<Ref<ArrayMesh>, Transform> &meshes);

    void _make_external_resources(Node *p_node, StringView p_base_path, bool p_make_animations, bool p_animations_as_text, bool p_keep_animations, bool p_make_materials, bool p_materials_as_text, bool p_keep_materials, bool p_make_meshes, bool p_meshes_as_text, Map<Ref<Animation>, Ref<Animation> > &p_animations, Map<Ref<Material>, Ref<Material> > &p_materials, Map<Ref<ArrayMesh>, Ref<ArrayMesh> > &p_meshes);


    void _create_clips(Node *scene, const Array &p_clips, bool p_bake_all);
    void _filter_anim_tracks(const Ref<Animation>& anim, Set<String> &keep);
    void _filter_tracks(Node *scene, StringView p_text);
    void _optimize_animations(Node *scene, float p_max_lin_error, float p_max_ang_error, float p_max_angle);

    Error import(StringView p_source_file, StringView p_save_path, const HashMap<StringName, Variant> &p_options, Vector<String> &r_missing_deps,
                 Vector<String> *r_platform_variants, Vector<String> *r_gen_files = nullptr, Variant *r_metadata = nullptr) override;

    Node *import_scene_from_other_importer(EditorSceneImporter *p_exception, StringView p_path, uint32_t p_flags, int p_bake_fps, uint32_t p_compress_flags);
    Ref<Animation> import_animation_from_other_importer(EditorSceneImporter *p_exception, StringView p_path, uint32_t p_flags, int p_bake_fps);

    ResourceImporterScene();
};

class EditorSceneImporterESCN : public EditorSceneImporterInterface {

public:
    uint32_t get_import_flags() const override;
    void get_extensions(Vector<String> &r_extensions) const override;
    Node *import_scene(StringView p_path, uint32_t p_flags, int p_bake_fps, uint32_t p_compress_flags, Vector<String> *r_missing_deps, Error *r_err = nullptr) override;
    Ref<Animation> import_animation(StringView, uint32_t, int) override;
};
