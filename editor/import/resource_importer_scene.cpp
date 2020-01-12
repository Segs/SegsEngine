/*************************************************************************/
/*  resource_importer_scene.cpp                                          */
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

#include "resource_importer_scene.h"

#include "core/io/resource_saver.h"
#include "core/method_bind.h"
#include "core/translation_helpers.h"
#include "editor/editor_node.h"
#include "scene/resources/packed_scene.h"

#include "scene/3d/collision_shape.h"
#include "scene/3d/mesh_instance.h"
#include "scene/3d/navigation.h"
#include "scene/3d/physics_body.h"
#include "scene/3d/portal.h"
#include "scene/3d/room_instance.h"
#include "scene/3d/vehicle_body.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/animation.h"
#include "scene/resources/box_shape.h"
#include "scene/resources/plane_shape.h"
#include "scene/resources/ray_shape.h"
#include "scene/resources/resource_format_text.h"
#include "scene/resources/sphere_shape.h"

IMPL_GDCLASS(EditorSceneImporter)
IMPL_GDCLASS(EditorScenePostImport)
IMPL_GDCLASS(ResourceImporterScene)

uint32_t EditorSceneImporter::get_import_flags() const {

    if (get_script_instance()) {
        return get_script_instance()->call("_get_import_flags");
    }

    ERR_FAIL_V(0)
}
void EditorSceneImporter::get_extensions(PODVector<se_string> &r_extensions) const {

    if (get_script_instance()) {
        Array arr = get_script_instance()->call("_get_extensions");
        for (int i = 0; i < arr.size(); i++) {
            r_extensions.push_back(arr[i].as<se_string>());
        }
        return;
    }

    ERR_FAIL()
}
Node *EditorSceneImporter::import_scene(
        se_string_view p_path, uint32_t p_flags, int p_bake_fps, PODVector<se_string> *r_missing_deps, Error *r_err) {

    if (get_script_instance()) {
        return get_script_instance()->call("_import_scene", p_path, p_flags, p_bake_fps);
    }

    ERR_FAIL_V(nullptr)
}

Ref<Animation> EditorSceneImporter::import_animation(se_string_view p_path, uint32_t p_flags, int p_bake_fps) {

    if (get_script_instance()) {
        return refFromRefPtr<Animation>(get_script_instance()->call("_import_animation", p_path, p_flags));
    }

    ERR_FAIL_V(Ref<Animation>())
}

// for documenters, these functions are useful when an importer calls an external conversion helper (like, fbx2gltf),
// and you want to load the resulting file

Node *EditorSceneImporter::import_scene_from_other_importer(se_string_view p_path, uint32_t p_flags, int p_bake_fps) {

    return ResourceImporterScene::get_singleton()->import_scene_from_other_importer(this, p_path, p_flags, p_bake_fps);
}

Ref<Animation> EditorSceneImporter::import_animation_from_other_importer(
        se_string_view p_path, uint32_t p_flags, int p_bake_fps) {

    return ResourceImporterScene::get_singleton()->import_animation_from_other_importer(
            this, p_path, p_flags, p_bake_fps);
}

void EditorSceneImporter::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("import_scene_from_other_importer", { "path", "flags", "bake_fps" }),
            &EditorSceneImporter::import_scene_from_other_importer);
    MethodBinder::bind_method(D_METHOD("import_animation_from_other_importer", { "path", "flags", "bake_fps" }),
            &EditorSceneImporter::import_animation_from_other_importer);

    BIND_VMETHOD(MethodInfo(VariantType::INT, "_get_import_flags"))
    BIND_VMETHOD(MethodInfo(VariantType::ARRAY, "_get_extensions"))

    MethodInfo mi = MethodInfo(VariantType::OBJECT, "_import_scene", PropertyInfo(VariantType::STRING, "path"),
            PropertyInfo(VariantType::INT, "flags"), PropertyInfo(VariantType::INT, "bake_fps"));
    mi.return_val.class_name = "Node";
    BIND_VMETHOD(mi)
    mi = MethodInfo(VariantType::OBJECT, "_import_animation", PropertyInfo(VariantType::STRING, "path"),
            PropertyInfo(VariantType::INT, "flags"), PropertyInfo(VariantType::INT, "bake_fps"));
    mi.return_val.class_name = "Animation";
    BIND_VMETHOD(mi)

    BIND_CONSTANT(IMPORT_SCENE)
    BIND_CONSTANT(IMPORT_ANIMATION)
    BIND_CONSTANT(IMPORT_ANIMATION_DETECT_LOOP)
    BIND_CONSTANT(IMPORT_ANIMATION_OPTIMIZE)
    BIND_CONSTANT(IMPORT_ANIMATION_FORCE_ALL_TRACKS_IN_ALL_CLIPS)
    BIND_CONSTANT(IMPORT_ANIMATION_KEEP_VALUE_TRACKS)
    BIND_CONSTANT(IMPORT_GENERATE_TANGENT_ARRAYS)
    BIND_CONSTANT(IMPORT_FAIL_ON_MISSING_DEPENDENCIES)
    BIND_CONSTANT(IMPORT_MATERIALS_IN_INSTANCES)
    BIND_CONSTANT(IMPORT_USE_COMPRESSION)
}

/////////////////////////////////
void EditorScenePostImport::_bind_methods() {

    BIND_VMETHOD(MethodInfo(VariantType::OBJECT, "post_import", PropertyInfo(VariantType::OBJECT, "scene")));
    MethodBinder::bind_method(D_METHOD("get_source_folder"), &EditorScenePostImport::get_source_folder);
    MethodBinder::bind_method(D_METHOD("get_source_file"), &EditorScenePostImport::get_source_file);
}

Node *EditorScenePostImport::post_import(Node *p_scene) {

    if (get_script_instance()) return get_script_instance()->call("post_import", Variant(p_scene));

    return p_scene;
}

const se_string &EditorScenePostImport::get_source_folder() const {

    return source_folder;
}

const se_string &EditorScenePostImport::get_source_file() const {

    return source_file;
}

void EditorScenePostImport::init(se_string_view p_source_folder, se_string_view p_source_file) {
    source_folder = p_source_folder;
    source_file = p_source_file;
}

EditorScenePostImport::EditorScenePostImport() {}

StringName ResourceImporterScene::get_importer_name() const {

    return "scene";
}

StringName ResourceImporterScene::get_visible_name() const {

    return "Scene";
}

void ResourceImporterScene::get_recognized_extensions(PODVector<se_string> &p_extensions) const {

    for (EditorSceneImporterInterface *E : importers) {
        E->get_extensions(p_extensions);
    }
}

StringName ResourceImporterScene::get_save_extension() const {
    return "scn";
}

StringName ResourceImporterScene::get_resource_type() const {

    return "PackedScene";
}

bool ResourceImporterScene::get_option_visibility(
        const StringName &p_option, const Map<StringName, Variant> &p_options) const {

    if (StringUtils::begins_with(p_option, "animation/")) {
        if (p_option != se_string_view("animation/import") && !bool(p_options.at("animation/import"))) return false;

        if (p_option == "animation/keep_custom_tracks" && int(p_options.at("animation/storage")) == 0) return false;

        if (StringUtils::begins_with(p_option, "animation/optimizer/") &&
                p_option != se_string_view("animation/optimizer/enabled") &&
                !bool(p_options.at("animation/optimizer/enabled")))
            return false;

        if (StringUtils::begins_with(p_option, "animation/clip_")) {
            int max_clip = p_options.at("animation/clips/amount");
            int clip =
                    StringUtils::to_int(StringUtils::get_slice(StringUtils::get_slice(p_option, '/', 1), '_', 1)) - 1;
            if (clip >= max_clip) return false;
        }
    }

    if (p_option == "materials/keep_on_reimport" && int(p_options.at("materials/storage")) == 0) {
        return false;
    }

    if (p_option == "meshes/lightmap_texel_size" && int(p_options.at("meshes/light_baking")) < 2) {
        return false;
    }

    return true;
}

int ResourceImporterScene::get_preset_count() const {
    return PRESET_MAX;
}
StringName ResourceImporterScene::get_preset_name(int p_idx) const {
    StringName translated;
    switch (p_idx) {
        case PRESET_SINGLE_SCENE: translated = TTR("Import as Single Scene"); break;
        case PRESET_SEPARATE_ANIMATIONS: translated = TTR("Import with Separate Animations"); break;
        case PRESET_SEPARATE_MATERIALS: translated = TTR("Import with Separate Materials"); break;
        case PRESET_SEPARATE_MESHES: translated = TTR("Import with Separate Objects"); break;
        case PRESET_SEPARATE_MESHES_AND_MATERIALS: translated = TTR("Import with Separate Objects+Materials"); break;
        case PRESET_SEPARATE_MESHES_AND_ANIMATIONS: translated = TTR("Import with Separate Objects+Animations"); break;
        case PRESET_SEPARATE_MATERIALS_AND_ANIMATIONS:
            translated = TTR("Import with Separate Materials+Animations");
            break;
        case PRESET_SEPARATE_MESHES_MATERIALS_AND_ANIMATIONS:
            translated = TTR("Import with Separate Objects+Materials+Animations");
            break;
        case PRESET_MULTIPLE_SCENES: translated = TTR("Import as Multiple Scenes"); break;
        case PRESET_MULTIPLE_SCENES_AND_MATERIALS: translated = TTR("Import as Multiple Scenes+Materials"); break;
    }
    return translated;
}

static bool _teststr(const se_string &p_what, const char *_str) {

    se_string p_str(_str);
    se_string what = p_what;

    // remove trailing spaces and numbers, some apps like blender add ".number" to duplicates so also compensate for
    // this
    while (what.length() && (what[what.length() - 1] >= '0' && what[what.length() - 1] <= '9' ||
                                    what[what.length() - 1] <= 32 || what[what.length() - 1] == '.')) {

        what = StringUtils::substr(what, 0, what.length() - 1);
    }

    if (StringUtils::findn(what, "$" + p_str) != se_string::npos) // blender and other stuff
        return true;
    if (StringUtils::ends_with(
                StringUtils::to_lower(what), "-" + p_str)) // collada only supports "_" and "-" besides letters
        return true;
    if (StringUtils::ends_with(
                StringUtils::to_lower(what), "_" + p_str)) // collada only supports "_" and "-" besides letters
        return true;
    return false;
}

static se_string_view _fixstr(se_string_view p_what, se_string_view p_str) {

    se_string_view what(p_what);

    // remove trailing spaces and numbers, some apps like blender add ".number" to duplicates so also compensate for
    // this
    while (what.length() && (what.back() >= '0' && what.back() <= '9' || what.back() <= 32 || what.back() == '.')) {

        what = StringUtils::substr(what, 0, what.length() - 1);
    }
    se_string test;
    test.push_back('v'); // placeholder
    test.append(p_str);
    se_string_view end = StringUtils::substr(p_what, what.length(), p_what.length() - what.length());
    test[0] = '$'; // blender and other stuff
    if (StringUtils::findn(what, test) != se_string::npos)
        return StringUtils::replace(what, test, "") + end;
    test[0] = '-'; // collada only supports "_" and "-" besides letters
    if (StringUtils::ends_with(StringUtils::to_lower(what), test))
        return se_string(StringUtils::substr(what, 0, what.length() - (p_str.length() + 1))) + end;
    test[0] = '_';
    if (StringUtils::ends_with(StringUtils::to_lower(what), test))
        return se_string(StringUtils::substr(what, 0, what.length() - (p_str.length() + 1))) + end;
    return what;
}
static void _gen_shape_list(const Ref<Mesh> &mesh, List<Ref<Shape>> &r_shape_list, bool p_convex) {

    if (!p_convex) {

        Ref<Shape> shape = mesh->create_trimesh_shape();
        r_shape_list.push_back(shape);
    } else {

        Vector<Ref<Shape>> cd = mesh->convex_decompose();
        if (!cd.empty()) {
            for (int i = 0; i < cd.size(); i++) {
                r_shape_list.push_back(cd[i]);
            }
        }
    }
}

Node *ResourceImporterScene::_fix_node(
        Node *p_node, Node *p_root, Map<Ref<Mesh>, List<Ref<Shape>>> &collision_map, LightBakeMode p_light_bake_mode) {

    // children first
    for (int i = 0; i < p_node->get_child_count(); i++) {

        Node *r = _fix_node(p_node->get_child(i), p_root, collision_map, p_light_bake_mode);
        if (!r) {
            i--; // was erased
        }
    }

    se_string name(p_node->get_name());

    bool isroot = p_node == p_root;

    if (!isroot && _teststr(name, "noimp")) {

        memdelete(p_node);
        return nullptr;
    }

    if (object_cast<MeshInstance>(p_node)) {

        MeshInstance *mi = object_cast<MeshInstance>(p_node);

        Ref<ArrayMesh> m = dynamic_ref_cast<ArrayMesh>(mi->get_mesh());

        if (m) {

            for (int i = 0; i < m->get_surface_count(); i++) {

                Ref<SpatialMaterial> mat = dynamic_ref_cast<SpatialMaterial>(m->surface_get_material(i));
                if (!mat) continue;

                if (_teststr(mat->get_name(), "alpha")) {

                    mat->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
                    mat->set_name(_fixstr(mat->get_name(), "alpha"));
                }
                if (_teststr(mat->get_name(), "vcol")) {

                    mat->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
                    mat->set_flag(SpatialMaterial::FLAG_SRGB_VERTEX_COLOR, true);
                    mat->set_name(_fixstr(mat->get_name(), "vcol"));
                }
            }
        }

        if (p_light_bake_mode != LIGHT_BAKE_DISABLED) {

            mi->set_flag(GeometryInstance::FLAG_USE_BAKED_LIGHT, true);
        }
    }

    if (object_cast<AnimationPlayer>(p_node)) {
        // remove animations referencing non-importable nodes
        AnimationPlayer *ap = object_cast<AnimationPlayer>(p_node);

        PODVector<StringName> anims;
        ap->get_animation_list(&anims);
        for (const StringName &E : anims) {

            Ref<Animation> anim = ap->get_animation(E);
            ERR_CONTINUE(not anim)
            for (int i = 0; i < anim->get_track_count(); i++) {
                NodePath path = anim->track_get_path(i);

                for (int j = 0; j < path.get_name_count(); j++) {
                    se_string node(path.get_name(j));
                    if (_teststr(node, "noimp")) {
                        anim->remove_track(i);
                        i--;
                        break;
                    }
                }
            }
        }
    }

    if (_teststr(name, "colonly") || _teststr(name, "convcolonly")) {

        if (isroot) return p_node;
        MeshInstance *mi = object_cast<MeshInstance>(p_node);
        if (mi) {
            Ref<Mesh> mesh = mi->get_mesh();

            if (mesh) {
                List<Ref<Shape>> shapes;
                se_string fixed_name;
                if (collision_map.contains(mesh)) {
                    shapes = collision_map[mesh];
                } else if (_teststr(name, "colonly")) {
                    _gen_shape_list(mesh, shapes, false);
                    collision_map[mesh] = shapes;
                } else if (_teststr(name, "convcolonly")) {
                    _gen_shape_list(mesh, shapes, true);
                    collision_map[mesh] = shapes;
                }

                if (_teststr(name, "colonly")) {
                    fixed_name = _fixstr(name, "colonly");
                } else if (_teststr(name, "convcolonly")) {
                    fixed_name = _fixstr(name, "convcolonly");
                }

                ERR_FAIL_COND_V(fixed_name.empty(), nullptr)

                if (!shapes.empty()) {

                    StaticBody *col = memnew(StaticBody);
                    col->set_transform(mi->get_transform());
                    col->set_name(fixed_name);
                    p_node->replace_by(col);
                    memdelete(p_node);
                    p_node = col;

                    int idx = 0;
                    for (List<Ref<Shape>>::Element *E = shapes.front(); E; E = E->next()) {

                        CollisionShape *cshape = memnew(CollisionShape);
                        cshape->set_shape(E->deref());
                        col->add_child(cshape);

                        cshape->set_name("shape" + itos(idx));
                        cshape->set_owner(col->get_owner());
                        idx++;
                    }
                }
            }

        } else if (p_node->has_meta("empty_draw_type")) {
            se_string empty_draw_type = se_string(p_node->get_meta("empty_draw_type"));
            StaticBody *sb = memnew(StaticBody);
            sb->set_name(_fixstr(name, "colonly"));
            object_cast<Spatial>(sb)->set_transform(object_cast<Spatial>(p_node)->get_transform());
            p_node->replace_by(sb);
            memdelete(p_node);
            p_node = nullptr;
            CollisionShape *colshape = memnew(CollisionShape);
            if (empty_draw_type == "CUBE") {
                BoxShape *boxShape = memnew(BoxShape);
                boxShape->set_extents(Vector3(1, 1, 1));
                colshape->set_shape(Ref<Shape>(boxShape));
                colshape->set_name("BoxShape");
            } else if (empty_draw_type == "SINGLE_ARROW") {
                RayShape *rayShape = memnew(RayShape);
                rayShape->set_length(1);
                colshape->set_shape(Ref<Shape>(rayShape));
                colshape->set_name("RayShape");
                object_cast<Spatial>(sb)->rotate_x(Math_PI / 2);
            } else if (empty_draw_type == "IMAGE") {
                PlaneShape *planeShape = memnew(PlaneShape);
                colshape->set_shape(Ref<Shape>(planeShape));
                colshape->set_name("PlaneShape");
            } else {
                SphereShape *sphereShape = memnew(SphereShape);
                sphereShape->set_radius(1);
                colshape->set_shape(Ref<Shape>(sphereShape));
                colshape->set_name("SphereShape");
            }
            sb->add_child(colshape);
            colshape->set_owner(sb->get_owner());
        }

    } else if (_teststr(name, "rigid") && object_cast<MeshInstance>(p_node)) {

        if (isroot) return p_node;

        MeshInstance *mi = object_cast<MeshInstance>(p_node);
        Ref<Mesh> mesh = mi->get_mesh();

        if (mesh) {
            List<Ref<Shape>> shapes;
            if (collision_map.contains(mesh)) {
                shapes = collision_map[mesh];
            } else {
                _gen_shape_list(mesh, shapes, true);
            }

            RigidBody *rigid_body = memnew(RigidBody);
            rigid_body->set_name(_fixstr(name, "rigid"));
            p_node->replace_by(rigid_body);
            rigid_body->set_transform(mi->get_transform());
            p_node = rigid_body;
            mi->set_name("mesh");
            mi->set_transform(Transform());
            rigid_body->add_child(mi);
            mi->set_owner(rigid_body->get_owner());

            int idx = 0;
            for (List<Ref<Shape>>::Element *E = shapes.front(); E; E = E->next()) {

                CollisionShape *cshape = memnew(CollisionShape);
                cshape->set_shape(E->deref());
                rigid_body->add_child(cshape);

                cshape->set_name("shape" + itos(idx));
                cshape->set_owner(p_node->get_owner());
                idx++;
            }
        }

    } else if ((_teststr(name, "col") || _teststr(name, "convcol")) && object_cast<MeshInstance>(p_node)) {

        MeshInstance *mi = object_cast<MeshInstance>(p_node);

        Ref<Mesh> mesh = mi->get_mesh();

        if (mesh) {
            List<Ref<Shape>> shapes;
            se_string fixed_name;
            if (collision_map.contains(mesh)) {
                shapes = collision_map[mesh];
            } else if (_teststr(name, "col")) {
                _gen_shape_list(mesh, shapes, false);
                collision_map[mesh] = shapes;
            } else if (_teststr(name, "convcol")) {
                _gen_shape_list(mesh, shapes, true);
                collision_map[mesh] = shapes;
            }

            if (_teststr(name, "col")) {
                fixed_name = _fixstr(name, "col");
            } else if (_teststr(name, "convcol")) {
                fixed_name = _fixstr(name, "convcol");
            }

            if (!fixed_name.empty()) {
                if (mi->get_parent() && !mi->get_parent()->has_node(NodePath(fixed_name))) {
                    mi->set_name(fixed_name);
                }
            }

            if (!shapes.empty()) {
                StaticBody *col = memnew(StaticBody);
                col->set_name("static_collision");
                mi->add_child(col);
                col->set_owner(mi->get_owner());

                int idx = 0;
                for (List<Ref<Shape>>::Element *E = shapes.front(); E; E = E->next()) {

                    CollisionShape *cshape = memnew(CollisionShape);
                    cshape->set_shape(E->deref());
                    col->add_child(cshape);

                    cshape->set_name("shape" + itos(idx));
                    cshape->set_owner(p_node->get_owner());

                    idx++;
                }
            }
        }

    } else if (_teststr(name, "navmesh") && object_cast<MeshInstance>(p_node)) {

        if (isroot) return p_node;

        MeshInstance *mi = object_cast<MeshInstance>(p_node);

        Ref<ArrayMesh> mesh = dynamic_ref_cast<ArrayMesh>(mi->get_mesh());
        ERR_FAIL_COND_V(not mesh, nullptr)
        NavigationMeshInstance *nmi = memnew(NavigationMeshInstance);

        nmi->set_name(_fixstr(name, "navmesh"));
        Ref<NavigationMesh> nmesh(make_ref_counted<NavigationMesh>());
        nmesh->create_from_mesh(mesh);
        nmi->set_navigation_mesh(nmesh);
        object_cast<Spatial>(nmi)->set_transform(mi->get_transform());
        p_node->replace_by(nmi);
        memdelete(p_node);
        p_node = nmi;
    } else if (_teststr(name, "vehicle")) {

        if (isroot) return p_node;

        Node *owner = p_node->get_owner();
        Spatial *s = object_cast<Spatial>(p_node);
        VehicleBody *bv = memnew(VehicleBody);
        se_string n(_fixstr(p_node->get_name(), "vehicle"));
        bv->set_name(n);
        p_node->replace_by(bv);
        p_node->set_name(n);
        bv->add_child(p_node);
        bv->set_owner(owner);
        p_node->set_owner(owner);
        bv->set_transform(s->get_transform());
        s->set_transform(Transform());

        p_node = bv;

    } else if (_teststr(name, "wheel")) {

        if (isroot) return p_node;

        Node *owner = p_node->get_owner();
        Spatial *s = object_cast<Spatial>(p_node);
        VehicleWheel *bv = memnew(VehicleWheel);
        se_string n(_fixstr(p_node->get_name(), "wheel"));
        bv->set_name(n);
        p_node->replace_by(bv);
        p_node->set_name(n);
        bv->add_child(p_node);
        bv->set_owner(owner);
        p_node->set_owner(owner);
        bv->set_transform(s->get_transform());
        s->set_transform(Transform());

        p_node = bv;

    } else if (object_cast<MeshInstance>(p_node)) {

        // last attempt, maybe collision inside the mesh data

        MeshInstance *mi = object_cast<MeshInstance>(p_node);

        Ref<ArrayMesh> mesh = dynamic_ref_cast<ArrayMesh>(mi->get_mesh());
        if (mesh) {

            List<Ref<Shape>> shapes;
            if (collision_map.contains(mesh)) {
                shapes = collision_map[mesh];
            } else if (_teststr(mesh->get_name(), "col")) {
                _gen_shape_list(mesh, shapes, false);
                collision_map[mesh] = shapes;
                mesh->set_name(_fixstr(mesh->get_name(), "col"));
            } else if (_teststr(mesh->get_name(), "convcol")) {
                _gen_shape_list(mesh, shapes, true);
                collision_map[mesh] = shapes;
                mesh->set_name(_fixstr(mesh->get_name(), "convcol"));
            }

            if (!shapes.empty()) {
                StaticBody *col = memnew(StaticBody);
                col->set_name("static_collision");
                p_node->add_child(col);
                col->set_owner(p_node->get_owner());

                int idx = 0;
                for (List<Ref<Shape>>::Element *E = shapes.front(); E; E = E->next()) {

                    CollisionShape *cshape = memnew(CollisionShape);
                    cshape->set_shape(E->deref());
                    col->add_child(cshape);

                    cshape->set_name("shape" + itos(idx));
                    cshape->set_owner(p_node->get_owner());
                    idx++;
                }
            }
        }
    }

    return p_node;
}

void ResourceImporterScene::_create_clips(Node *scene, const Array &p_clips, bool p_bake_all) {

    if (!scene->has_node(NodePath("AnimationPlayer"))) return;

    Node *n = scene->get_node(NodePath("AnimationPlayer"));
    ERR_FAIL_COND(!n)
    AnimationPlayer *anim = object_cast<AnimationPlayer>(n);
    ERR_FAIL_COND(!anim)

    if (!anim->has_animation("default")) return;

    Ref<Animation> default_anim = anim->get_animation("default");

    for (int i = 0; i < p_clips.size(); i += 4) {

        se_string name = p_clips[i];
        float from = p_clips[i + 1];
        float to = p_clips[i + 2];
        bool loop = p_clips[i + 3];
        if (from >= to) continue;

        Ref<Animation> new_anim(make_ref_counted<Animation>());

        for (int j = 0; j < default_anim->get_track_count(); j++) {

            List<float> keys;
            int kc = default_anim->track_get_key_count(j);
            int dtrack = -1;
            for (int k = 0; k < kc; k++) {

                float kt = default_anim->track_get_key_time(j, k);
                if (kt >= from && kt < to) {

                    // found a key within range, so create track
                    if (dtrack == -1) {
                        new_anim->add_track(default_anim->track_get_type(j));
                        dtrack = new_anim->get_track_count() - 1;
                        new_anim->track_set_path(dtrack, default_anim->track_get_path(j));

                        if (kt > from + 0.01f && k > 0) {

                            if (default_anim->track_get_type(j) == Animation::TYPE_TRANSFORM) {
                                Quat q;
                                Vector3 p;
                                Vector3 s;
                                default_anim->transform_track_interpolate(j, from, &p, &q, &s);
                                new_anim->transform_track_insert_key(dtrack, 0, p, q, s);
                            }
                            if (default_anim->track_get_type(j) == Animation::TYPE_VALUE) {
                                Variant var = default_anim->value_track_interpolate(j, from);
                                new_anim->track_insert_key(dtrack, 0, var);
                            }
                        }
                    }

                    if (default_anim->track_get_type(j) == Animation::TYPE_TRANSFORM) {
                        Quat q;
                        Vector3 p;
                        Vector3 s;
                        default_anim->transform_track_get_key(j, k, &p, &q, &s);
                        new_anim->transform_track_insert_key(dtrack, kt - from, p, q, s);
                    }
                    if (default_anim->track_get_type(j) == Animation::TYPE_VALUE) {
                        Variant var = default_anim->track_get_key_value(j, k);
                        new_anim->track_insert_key(dtrack, kt - from, var);
                    }
                }

                if (dtrack != -1 && kt >= to) {

                    if (default_anim->track_get_type(j) == Animation::TYPE_TRANSFORM) {
                        Quat q;
                        Vector3 p;
                        Vector3 s;
                        default_anim->transform_track_interpolate(j, to, &p, &q, &s);
                        new_anim->transform_track_insert_key(dtrack, to - from, p, q, s);
                    }
                    if (default_anim->track_get_type(j) == Animation::TYPE_VALUE) {
                        Variant var = default_anim->value_track_interpolate(j, to);
                        new_anim->track_insert_key(dtrack, to - from, var);
                    }
                }
            }

            if (dtrack == -1 && p_bake_all) {
                new_anim->add_track(default_anim->track_get_type(j));
                dtrack = new_anim->get_track_count() - 1;
                new_anim->track_set_path(dtrack, default_anim->track_get_path(j));
                if (default_anim->track_get_type(j) == Animation::TYPE_TRANSFORM) {

                    Quat q;
                    Vector3 p;
                    Vector3 s;
                    default_anim->transform_track_interpolate(j, from, &p, &q, &s);
                    new_anim->transform_track_insert_key(dtrack, 0, p, q, s);
                    default_anim->transform_track_interpolate(j, to, &p, &q, &s);
                    new_anim->transform_track_insert_key(dtrack, to - from, p, q, s);
                }
                if (default_anim->track_get_type(j) == Animation::TYPE_VALUE) {
                    Variant var = default_anim->value_track_interpolate(j, from);
                    new_anim->track_insert_key(dtrack, 0, var);
                    Variant to_var = default_anim->value_track_interpolate(j, to);
                    new_anim->track_insert_key(dtrack, to - from, to_var);
                }
            }
        }

        new_anim->set_loop(loop);
        new_anim->set_length(to - from);
        anim->add_animation(StringName(name), new_anim);
    }

    anim->remove_animation("default"); // remove default (no longer needed)
}

void ResourceImporterScene::_filter_anim_tracks(const Ref<Animation> &anim, Set<se_string> &keep) {

    const Ref<Animation> &a(anim);
    ERR_FAIL_COND(!a)

    for (int j = 0; j < a->get_track_count(); j++) {

        se_string path(a->track_get_path(j));

        if (!keep.contains(path)) {
            a->remove_track(j);
            j--;
        }
    }
}

void ResourceImporterScene::_filter_tracks(Node *scene, se_string_view p_text) {

    if (!scene->has_node(NodePath("AnimationPlayer"))) return;
    Node *n = scene->get_node(NodePath("AnimationPlayer"));
    ERR_FAIL_COND(!n)
    AnimationPlayer *anim = object_cast<AnimationPlayer>(n);
    ERR_FAIL_COND(!anim)

    PODVector<se_string_view> strings = StringUtils::split(p_text, '\n');
    for (se_string_view &sv : strings) {

        sv = StringUtils::strip_edges(sv);
    }

    PODVector<StringName> anim_names;
    anim->get_animation_list(&anim_names);
    for (const StringName &E : anim_names) {

        se_string_view name = E;
        bool valid_for_this = false;
        bool valid = false;

        Set<se_string> keep;
        Set<se_string> keep_local;

        for (int i = 0; i < strings.size(); i++) {

            if (StringUtils::begins_with(strings[i], "@")) {

                valid_for_this = false;
                for (const se_string &F : keep_local) {
                    keep.insert(F);
                }
                keep_local.clear();

                PODVector<se_string_view> filters =
                        StringUtils::split(StringUtils::substr(strings[i], 1, strings[i].length()), ',');
                for (se_string_view val : filters) {

                    se_string_view fname = StringUtils::strip_edges(val);
                    if (fname.empty()) continue;
                    int fc = fname[0];
                    bool plus;
                    if (fc == '+')
                        plus = true;
                    else if (fc == '-')
                        plus = false;
                    else
                        continue;

                    se_string_view filter = StringUtils::strip_edges(StringUtils::substr(fname, 1, fname.length()));

                    if (!StringUtils::match(name, filter, StringUtils::CaseInsensitive)) continue;
                    valid_for_this = plus;
                }

                if (valid_for_this) valid = true;

            } else if (valid_for_this) {

                Ref<Animation> a = anim->get_animation(StringName(name));
                if (not a) continue;

                for (int j = 0; j < a->get_track_count(); j++) {

                    se_string path(a->track_get_path(j));

                    se_string_view tname = strings[i];
                    if (tname.empty()) continue;
                    int fc = tname[0];
                    bool plus;
                    if (fc == '+')
                        plus = true;
                    else if (fc == '-')
                        plus = false;
                    else
                        continue;

                    se_string_view filter = StringUtils::strip_edges(StringUtils::substr(tname, 1, tname.length()));

                    if (!StringUtils::match(path, filter, StringUtils::CaseInsensitive)) continue;

                    if (plus)
                        keep_local.insert(path);
                    else if (!keep.contains(path)) {
                        keep_local.erase(path);
                    }
                }
            }
        }

        if (valid) {
            for (const se_string &F : keep_local) {
                keep.insert(F);
            }
            _filter_anim_tracks(anim->get_animation(StringName(name)), keep);
        }
    }
}

void ResourceImporterScene::_optimize_animations(
        Node *scene, float p_max_lin_error, float p_max_ang_error, float p_max_angle) {

    if (!scene->has_node(NodePath("AnimationPlayer"))) return;
    Node *n = scene->get_node(NodePath("AnimationPlayer"));
    ERR_FAIL_COND(!n)
    AnimationPlayer *anim = object_cast<AnimationPlayer>(n);
    ERR_FAIL_COND(!anim)

    PODVector<StringName> anim_names;
    anim->get_animation_list(&anim_names);
    for (const StringName &E : anim_names) {

        Ref<Animation> a = anim->get_animation(E);
        a->optimize(p_max_lin_error, p_max_ang_error, Math::deg2rad(p_max_angle));
    }
}

static se_string _make_extname(se_string_view p_str) {

    se_string ext_name(p_str);
    ext_name.replace('.', '_');
    ext_name.replace(':', '_');
    ext_name.replace('\"', '_');
    ext_name.replace('<', '_');
    ext_name.replace('>', '_');
    ext_name.replace('/', '_');
    ext_name.replace('|', '_');
    ext_name.replace('\\', '_');
    ext_name.replace('?', '_');
    ext_name.replace('*', '_');

    return ext_name;
}

void ResourceImporterScene::_find_meshes(Node *p_node, Map<Ref<ArrayMesh>, Transform> &meshes) {

    ListPOD<PropertyInfo> pi;
    p_node->get_property_list(&pi);

    MeshInstance *mi = object_cast<MeshInstance>(p_node);

    if (mi) {

        Ref<ArrayMesh> mesh = dynamic_ref_cast<ArrayMesh>(mi->get_mesh());

        if (mesh && !meshes.contains(mesh)) {
            Spatial *s = mi;
            Transform transform;
            while (s) {
                transform = transform * s->get_transform();
                s = s->get_parent_spatial();
            }

            meshes[mesh] = transform;
        }
    }
    for (int i = 0; i < p_node->get_child_count(); i++) {

        _find_meshes(p_node->get_child(i), meshes);
    }
}

void ResourceImporterScene::_make_external_resources(Node *p_node, se_string_view p_base_path, bool p_make_animations,
        bool p_animations_as_text, bool p_keep_animations, bool p_make_materials, bool p_materials_as_text,
        bool p_keep_materials, bool p_make_meshes, bool p_meshes_as_text,
        Map<Ref<Animation>, Ref<Animation>> &p_animations, Map<Ref<Material>, Ref<Material>> &p_materials,
        Map<Ref<ArrayMesh>, Ref<ArrayMesh>> &p_meshes) {

    ListPOD<PropertyInfo> pi;

    if (p_make_animations) {
        if (object_cast<AnimationPlayer>(p_node)) {
            AnimationPlayer *ap = object_cast<AnimationPlayer>(p_node);

            PODVector<StringName> anims;
            ap->get_animation_list(&anims);
            for (const StringName &E : anims) {

                Ref<Animation> anim = ap->get_animation(E);
                ERR_CONTINUE(not anim)

                if (!p_animations.contains(anim)) {

                    // mark what comes from the file first, this helps eventually keep user data
                    for (int i = 0; i < anim->get_track_count(); i++) {
                        anim->track_set_imported(i, true);
                    }
                    se_string ext_name;

                    if (p_animations_as_text) {
                        ext_name = PathUtils::plus_file(p_base_path, _make_extname(E) + ".tres");
                    } else {
                        ext_name = PathUtils::plus_file(p_base_path, _make_extname(E) + ".anim");
                    }
                    if (FileAccess::exists(ext_name) && p_keep_animations) {
                        // try to keep custom animation tracks
                        Ref<Animation> old_anim = dynamic_ref_cast<Animation>(
                                ResourceLoader::load(ext_name, se_string("Animation"), true));
                        if (old_anim) {
                            // meergeee
                            for (int i = 0; i < old_anim->get_track_count(); i++) {
                                if (!old_anim->track_is_imported(i)) {
                                    old_anim->copy_track(i, anim);
                                }
                            }
                            anim->set_loop(old_anim->has_loop());
                        }
                    }

                    anim->set_path(ext_name, true); // if not set, then its never saved externally
                    ResourceSaver::save(ext_name, anim, ResourceSaver::FLAG_CHANGE_PATH);
                    p_animations[anim] = anim;
                }
            }
        }
    }

    p_node->get_property_list(&pi);

    for (const PropertyInfo &E : pi) {

        if (E.type == VariantType::OBJECT) {

            Ref<Material> mat(p_node->get(E.name));

            if (p_make_materials && mat && !mat->get_name().empty()) {

                if (!p_materials.contains(mat)) {
                    se_string ext_name;

                    if (p_materials_as_text) {
                        ext_name = PathUtils::plus_file(p_base_path, _make_extname(mat->get_name()) + ".tres");
                    } else {
                        ext_name = PathUtils::plus_file(p_base_path, _make_extname(mat->get_name()) + ".material");
                    }

                    if (p_keep_materials && FileAccess::exists(ext_name)) {
                        // if exists, use it
                        p_materials[mat] = dynamic_ref_cast<Material>(ResourceLoader::load(ext_name));
                    } else {

                        ResourceSaver::save(ext_name, mat, ResourceSaver::FLAG_CHANGE_PATH);
                        p_materials[mat] = dynamic_ref_cast<Material>(
                                ResourceLoader::load(ext_name, "", true)); // disable loading from the cache.
                    }
                }

                if (p_materials[mat] != mat) {

                    p_node->set(E.name, p_materials[mat]);
                }
            } else {

                Ref<ArrayMesh> mesh(p_node->get(E.name));

                if (mesh) {

                    bool mesh_just_added = false;

                    if (p_make_meshes) {

                        if (!p_meshes.contains(mesh)) {

                            // meshes are always overwritten, keeping them is not practical
                            se_string ext_name;

                            if (p_meshes_as_text) {
                                ext_name = PathUtils::plus_file(p_base_path, _make_extname(mesh->get_name()) + ".tres");
                            } else {
                                ext_name = PathUtils::plus_file(p_base_path, _make_extname(mesh->get_name()) + ".mesh");
                            }

                            ResourceSaver::save(ext_name, mesh, ResourceSaver::FLAG_CHANGE_PATH);
                            p_meshes[mesh] = dynamic_ref_cast<ArrayMesh>(ResourceLoader::load(ext_name));
                            p_node->set(E.name, p_meshes[mesh]);
                            mesh_just_added = true;
                        }
                    }

                    if (p_make_materials) {

                        if (mesh_just_added || !p_meshes.contains(mesh)) {

                            for (int i = 0; i < mesh->get_surface_count(); i++) {
                                mat = mesh->surface_get_material(i);

                                if (not mat) continue;
                                if (mat->get_name().empty()) continue;

                                if (!p_materials.contains(mat)) {
                                    se_string ext_name;
                                    if (p_materials_as_text) {
                                        ext_name = PathUtils::plus_file(
                                                p_base_path, _make_extname(mat->get_name()) + ".tres");
                                    } else {
                                        ext_name = PathUtils::plus_file(
                                                p_base_path, _make_extname(mat->get_name()) + ".material");
                                    }

                                    if (p_keep_materials && FileAccess::exists(ext_name)) {
                                        // if exists, use it
                                        p_materials[mat] = dynamic_ref_cast<Material>(ResourceLoader::load(ext_name));
                                    } else {

                                        ResourceSaver::save(ext_name, mat, ResourceSaver::FLAG_CHANGE_PATH);
                                        p_materials[mat] = dynamic_ref_cast<Material>(ResourceLoader::load(
                                                ext_name, "", true)); // disable loading from the cache.
                                    }
                                }

                                if (p_materials[mat] != mat) {

                                    mesh->surface_set_material(i, p_materials[mat]);

                                    // re-save the mesh since a material is now assigned
                                    if (p_make_meshes) {
                                        se_string ext_name;

                                        if (p_meshes_as_text) {
                                            ext_name = PathUtils::plus_file(
                                                    p_base_path, _make_extname(mesh->get_name()) + ".tres");
                                        } else {
                                            ext_name = PathUtils::plus_file(
                                                    p_base_path, _make_extname(mesh->get_name()) + ".mesh");
                                        }
                                        ResourceSaver::save(ext_name, mesh, ResourceSaver::FLAG_CHANGE_PATH);
                                        p_meshes[mesh] = dynamic_ref_cast<ArrayMesh>(ResourceLoader::load(ext_name));
                                    }
                                }
                            }
                            if (!p_make_meshes) {
                                p_meshes[mesh] = Ref<ArrayMesh>(); // save it anyway, so it won't be checked again
                            }
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < p_node->get_child_count(); i++) {

        _make_external_resources(p_node->get_child(i), p_base_path, p_make_animations, p_animations_as_text,
                p_keep_animations, p_make_materials, p_materials_as_text, p_keep_materials, p_make_meshes,
                p_meshes_as_text, p_animations, p_materials, p_meshes);
    }
}

void ResourceImporterScene::get_import_options(ListPOD<ImportOption> *r_options, int p_preset) const {

    r_options->push_back(ImportOption(
            PropertyInfo(VariantType::STRING, "nodes/root_type", PROPERTY_HINT_TYPE_STRING, "Node"), "Spatial"));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::STRING, "nodes/root_name"), "Scene Root"));

    PODVector<se_string> script_extentions;
    ResourceLoader::get_recognized_extensions_for_type("Script", script_extentions);

    se_string script_ext_hint;

    for (const se_string &E : script_extentions) {
        if (!script_ext_hint.empty()) script_ext_hint += ",";
        script_ext_hint += "*." + E;
    }

    bool materials_out = p_preset == PRESET_SEPARATE_MATERIALS || p_preset == PRESET_SEPARATE_MESHES_AND_MATERIALS ||
                         p_preset == PRESET_MULTIPLE_SCENES_AND_MATERIALS ||
                         p_preset == PRESET_SEPARATE_MATERIALS_AND_ANIMATIONS ||
                         p_preset == PRESET_SEPARATE_MESHES_MATERIALS_AND_ANIMATIONS;
    bool meshes_out = p_preset == PRESET_SEPARATE_MESHES || p_preset == PRESET_SEPARATE_MESHES_AND_MATERIALS ||
                      p_preset == PRESET_SEPARATE_MESHES_AND_ANIMATIONS ||
                      p_preset == PRESET_SEPARATE_MESHES_MATERIALS_AND_ANIMATIONS;
    bool scenes_out = p_preset == PRESET_MULTIPLE_SCENES || p_preset == PRESET_MULTIPLE_SCENES_AND_MATERIALS;
    bool animations_out = p_preset == PRESET_SEPARATE_ANIMATIONS || p_preset == PRESET_SEPARATE_MESHES_AND_ANIMATIONS ||
                          p_preset == PRESET_SEPARATE_MATERIALS_AND_ANIMATIONS ||
                          p_preset == PRESET_SEPARATE_MESHES_MATERIALS_AND_ANIMATIONS;

    r_options->push_back(ImportOption(
            PropertyInfo(VariantType::REAL, "nodes/root_scale", PROPERTY_HINT_RANGE, "0.001,1000,0.001"), 1.0));
    r_options->push_back(ImportOption(
            PropertyInfo(VariantType::STRING, "nodes/custom_script", PROPERTY_HINT_FILE, StringName(script_ext_hint)),
            ""));
    r_options->push_back(ImportOption(
            PropertyInfo(VariantType::INT, "nodes/storage", PROPERTY_HINT_ENUM, "Single Scene,Instanced Sub-Scenes"),
            scenes_out ? 1 : 0));
    r_options->push_back(
            ImportOption(PropertyInfo(VariantType::INT, "materials/location", PROPERTY_HINT_ENUM, "Node,Mesh"),
                    meshes_out || materials_out ? 1 : 0));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "materials/storage", PROPERTY_HINT_ENUM,
                                              "Built-In,Files (.material),Files (.tres)",
                                              PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED),
            materials_out ? 1 : 0));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "materials/keep_on_reimport"), materials_out));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "meshes/compress"), true));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "meshes/ensure_tangents"), true));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "meshes/storage", PROPERTY_HINT_ENUM,
                                              "Built-In,Files (.mesh),Files (.tres)"),
            meshes_out ? 1 : 0));
    r_options->push_back(ImportOption(
            PropertyInfo(VariantType::INT, "meshes/light_baking", PROPERTY_HINT_ENUM, "Disabled,Enable,Gen Lightmaps",
                    PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED),
            0));
    r_options->push_back(ImportOption(
            PropertyInfo(VariantType::REAL, "meshes/lightmap_texel_size", PROPERTY_HINT_RANGE, "0.001,100,0.001"),
            0.1));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "external_files/store_in_subdir"), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "animation/import", PROPERTY_HINT_NONE, "",
                                              PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED),
            true));
    r_options->push_back(
            ImportOption(PropertyInfo(VariantType::REAL, "animation/fps", PROPERTY_HINT_RANGE, "1,120,1"), 15));
    r_options->push_back(ImportOption(
            PropertyInfo(VariantType::STRING, "animation/filter_script", PROPERTY_HINT_MULTILINE_TEXT), ""));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "animation/storage", PROPERTY_HINT_ENUM,
                                              "Built-In,Files (.anim),Files (.tres)",
                                              PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED),
            animations_out));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "animation/keep_custom_tracks"), animations_out));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "animation/optimizer/enabled", PROPERTY_HINT_NONE,
                                              "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED),
            true));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::REAL, "animation/optimizer/max_linear_error"), 0.05));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::REAL, "animation/optimizer/max_angular_error"), 0.01));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::REAL, "animation/optimizer/max_angle"), 22));
    r_options->push_back(
            ImportOption(PropertyInfo(VariantType::BOOL, "animation/optimizer/remove_unused_tracks"), true));
    r_options->push_back(
            ImportOption(PropertyInfo(VariantType::INT, "animation/clips/amount", PROPERTY_HINT_RANGE, "0,256,1",
                                 PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED),
                    0));
    for (int i = 0; i < 256; i++) {
        r_options->push_back(ImportOption(
                PropertyInfo(VariantType::STRING, StringName("animation/clip_" + itos(i + 1) + "/name")), ""));
        r_options->push_back(ImportOption(
                PropertyInfo(VariantType::INT, StringName("animation/clip_" + itos(i + 1) + "/start_frame")), 0));
        r_options->push_back(ImportOption(
                PropertyInfo(VariantType::INT, StringName("animation/clip_" + itos(i + 1) + "/end_frame")), 0));
        r_options->push_back(ImportOption(
                PropertyInfo(VariantType::BOOL, StringName("animation/clip_" + itos(i + 1) + "/loops")), false));
    }
}

void ResourceImporterScene::_replace_owner(Node *p_node, Node *p_scene, Node *p_new_owner) {

    if (p_node != p_new_owner && p_node->get_owner() == p_scene) {
        p_node->set_owner(p_new_owner);
    }

    for (int i = 0; i < p_node->get_child_count(); i++) {
        Node *n = p_node->get_child(i);
        _replace_owner(n, p_scene, p_new_owner);
    }
}

Node *ResourceImporterScene::import_scene_from_other_importer(
        EditorSceneImporter *p_exception, se_string_view p_path, uint32_t p_flags, int p_bake_fps) {

    EditorSceneImporterInterface *importer = nullptr;
    se_string ext = StringUtils::to_lower(PathUtils::get_extension(p_path));

    for (EditorSceneImporterInterface *E : importers) {

        if (E == p_exception) continue;
        PODVector<se_string> extensions;
        E->get_extensions(extensions);

        for (size_t i = 0, fin = extensions.size(); i < fin; ++i) {

            if (StringUtils::to_lower(extensions[i]) == ext) {

                importer = E;
                break;
            }
        }

        if (importer != nullptr) break;
    }

    ERR_FAIL_COND_V(importer == nullptr, nullptr)

    PODVector<se_string> missing;
    Error err;
    return importer->import_scene(p_path, p_flags, p_bake_fps, &missing, &err);
}

Ref<Animation> ResourceImporterScene::import_animation_from_other_importer(
        EditorSceneImporter *p_exception, se_string_view p_path, uint32_t p_flags, int p_bake_fps) {

    EditorSceneImporterInterface *importer = nullptr;
    se_string ext = StringUtils::to_lower(PathUtils::get_extension(p_path));

    for (EditorSceneImporterInterface *E : importers) {

        if (E == p_exception) continue;
        PODVector<se_string> extensions;
        E->get_extensions(extensions);

        for (size_t i = 0, fin = extensions.size(); i < fin; ++i) {

            if (StringUtils::to_lower(extensions[i]) == ext) {

                importer = E;
                break;
            }
        }

        if (importer != nullptr) break;
    }

    ERR_FAIL_COND_V(importer == nullptr, Ref<Animation>())

    return importer->import_animation(p_path, p_flags, p_bake_fps);
}

Error ResourceImporterScene::import(se_string_view p_source_file, se_string_view p_save_path,
        const Map<StringName, Variant> &p_options, List<se_string> *r_platform_variants, List<se_string> *r_gen_files,
        Variant *r_metadata) {

    se_string_view src_path = p_source_file;

    EditorSceneImporterInterface *importer = nullptr;
    se_string ext = StringUtils::to_lower(PathUtils::get_extension(src_path));

    EditorProgress progress(("import"), TTR("Import Scene"), 104);
    progress.step(TTR("Importing Scene..."), 0);

    for (EditorSceneImporterInterface *E : importers) {

        PODVector<se_string> extensions;
        E->get_extensions(extensions);

        for (size_t i = 0, fin = extensions.size(); i < fin; ++i) {

            if (StringUtils::to_lower(extensions[i]) == ext) {

                importer = E;
                break;
            }
        }

        if (importer != nullptr) break;
    }

    ERR_FAIL_COND_V(nullptr == importer, ERR_FILE_UNRECOGNIZED)

    float fps = p_options.at("animation/fps");

    int import_flags = EditorSceneImporter::IMPORT_ANIMATION_DETECT_LOOP;
    if (!bool(p_options.at("animation/optimizer/remove_unused_tracks")))
        import_flags |= EditorSceneImporter::IMPORT_ANIMATION_FORCE_ALL_TRACKS_IN_ALL_CLIPS;

    if (bool(p_options.at("animation/import"))) import_flags |= EditorSceneImporter::IMPORT_ANIMATION;

    if (int(p_options.at("meshes/compress"))) import_flags |= EditorSceneImporter::IMPORT_USE_COMPRESSION;

    if (bool(p_options.at("meshes/ensure_tangents")))
        import_flags |= EditorSceneImporter::IMPORT_GENERATE_TANGENT_ARRAYS;

    if (int(p_options.at("materials/location")) == 0)
        import_flags |= EditorSceneImporter::IMPORT_MATERIALS_IN_INSTANCES;

    Error err = OK;
    PODVector<se_string> missing_deps; // for now, not much will be done with this
    Node *scene = importer->import_scene(src_path, import_flags, fps, &missing_deps, &err);
    if (!scene || err != OK) {
        return err;
    }

    se_string root_type_tx = p_options.at("nodes/root_type");
    StringName root_type = StringName(StringUtils::split(
            root_type_tx, ' ')[0]); // full root_type is "ClassName (filename.gd)" for a script global class.

    Ref<Script> root_script;
    if (ScriptServer::is_global_class(root_type)) {
        root_script = dynamic_ref_cast<Script>(ResourceLoader::load(ScriptServer::get_global_class_path(root_type)));
        root_type = ScriptServer::get_global_class_base(root_type);
    }

    if (root_type != se_string_view("Spatial")) {
        Node *base_node = object_cast<Node>(ClassDB::instance(root_type));

        if (base_node) {

            scene->replace_by(base_node);
            memdelete(scene);
            scene = base_node;
        }
    }

    if (root_script) {
        scene->set_script(Variant(root_script));
    }

    if (object_cast<Spatial>(scene)) {
        float root_scale = p_options.at("nodes/root_scale");
        object_cast<Spatial>(scene)->scale(Vector3(root_scale, root_scale, root_scale));
    }

    if (p_options.at("nodes/root_name") != "Scene Root")
        scene->set_name(p_options.at("nodes/root_name").as<se_string>());
    else
        scene->set_name(PathUtils::get_basename(PathUtils::get_file(p_save_path)));

    err = OK;

    se_string animation_filter(StringUtils::strip_edges(p_options.at("animation/filter_script").as<se_string>()));

    bool use_optimizer = p_options.at("animation/optimizer/enabled");
    float anim_optimizer_linerr = p_options.at("animation/optimizer/max_linear_error");
    float anim_optimizer_angerr = p_options.at("animation/optimizer/max_angular_error");
    float anim_optimizer_maxang = p_options.at("animation/optimizer/max_angle");
    int light_bake_mode = p_options.at("meshes/light_baking");

    Map<Ref<Mesh>, List<Ref<Shape>>> collision_map;

    scene = _fix_node(scene, scene, collision_map, LightBakeMode(light_bake_mode));

    if (use_optimizer) {
        _optimize_animations(scene, anim_optimizer_linerr, anim_optimizer_angerr, anim_optimizer_maxang);
    }

    Array animation_clips;
    {

        int clip_count = p_options.at("animation/clips/amount");

        for (int i = 0; i < clip_count; i++) {
            se_string name = p_options.at(StringName("animation/clip_" + itos(i + 1) + "/name"));
            int from_frame = p_options.at(StringName("animation/clip_" + itos(i + 1) + "/start_frame"));
            int end_frame = p_options.at(StringName("animation/clip_" + itos(i + 1) + "/end_frame"));
            bool loop = p_options.at(StringName("animation/clip_" + itos(i + 1) + "/loops"));

            animation_clips.push_back(name);
            animation_clips.push_back(from_frame / fps);
            animation_clips.push_back(end_frame / fps);
            animation_clips.push_back(loop);
        }
    }
    if (!animation_clips.empty()) {
        _create_clips(scene, animation_clips, !bool(p_options.at("animation/optimizer/remove_unused_tracks")));
    }

    if (!animation_filter.empty()) {
        _filter_tracks(scene, animation_filter);
    }

    bool external_animations =
            int(p_options.at("animation/storage")) == 1 || int(p_options.at("animation/storage")) == 2;
    bool external_animations_as_text = int(p_options.at("animation/storage")) == 2;
    bool keep_custom_tracks = p_options.at("animation/keep_custom_tracks");
    bool external_materials =
            int(p_options.at("materials/storage")) == 1 || int(p_options.at("materials/storage")) == 2;
    bool external_materials_as_text = int(p_options.at("materials/storage")) == 2;
    bool external_meshes = int(p_options.at("meshes/storage")) == 1 || int(p_options.at("meshes/storage")) == 2;
    bool external_meshes_as_text = int(p_options.at("meshes/storage")) == 2;
    bool external_scenes = int(p_options.at("nodes/storage")) == 1;

    se_string base_path(PathUtils::get_base_dir(p_source_file));

    if (external_animations || external_materials || external_meshes || external_scenes) {

        if (bool(p_options.at("external_files/store_in_subdir"))) {
            se_string_view subdir_name = PathUtils::get_basename(PathUtils::get_file(p_source_file));
            DirAccess *da = DirAccess::open(base_path);
            Error err2 = da->make_dir(subdir_name);
            memdelete(da);
            ERR_FAIL_COND_V_MSG(err2 != OK && err2 != ERR_ALREADY_EXISTS, err2,
                    "Cannot make directory '" + se_string(subdir_name) + "'.")
            base_path = PathUtils::plus_file(base_path, subdir_name);
        }
    }

    if (light_bake_mode == 2 /* || generate LOD */) {

        Map<Ref<ArrayMesh>, Transform> meshes;
        _find_meshes(scene, meshes);

        if (light_bake_mode == 2) {

            float texel_size = p_options.at("meshes/lightmap_texel_size");
            texel_size = MAX(0.001f, texel_size);

            EditorProgress progress2(("gen_lightmaps"), TTR("Generating Lightmaps"), meshes.size());
            int step = 0;
            for (eastl::pair<const Ref<ArrayMesh>, Transform> &E : meshes) {

                Ref<ArrayMesh> mesh = E.first;
                se_string name(mesh->get_name());
                if (name.empty()) { // should not happen but..
                    name = "Mesh " + itos(step);
                }

                progress2.step(TTR("Generating for Mesh: ") + se_string_view(name + " (" + itos(step) + "/" +
                                                                                     itos(meshes.size()) + ")"),
                        step);

                Error err2 = mesh->lightmap_unwrap(E.second, texel_size);
                if (err2 != OK) {
                    EditorNode::add_io_error(StringName(
                            "Mesh '" + name + "' failed lightmap generation. Please fix geometry."));
                }
                step++;
            }
        }
    }

    if (external_animations || external_materials || external_meshes) {
        Map<Ref<Animation>, Ref<Animation>> anim_map;
        Map<Ref<Material>, Ref<Material>> mat_map;
        Map<Ref<ArrayMesh>, Ref<ArrayMesh>> mesh_map;

        bool keep_materials = bool(p_options.at("materials/keep_on_reimport"));

        _make_external_resources(scene, base_path, external_animations, external_animations_as_text, keep_custom_tracks,
                external_materials, external_materials_as_text, keep_materials, external_meshes,
                external_meshes_as_text, anim_map, mat_map, mesh_map);
    }

    progress.step(TTR("Running Custom Script..."), 2);

    se_string post_import_script_path(p_options.at("nodes/custom_script"));
    Ref<EditorScenePostImport> post_import_script;

    if (!post_import_script_path.empty()) {
        Ref<Script> scr = dynamic_ref_cast<Script>(ResourceLoader::load(post_import_script_path));
        if (not scr) {
            EditorNode::add_io_error(
                    TTR("Couldn't load post-import script:") + se_string_view(" " + post_import_script_path));
        } else {

            post_import_script = make_ref_counted<EditorScenePostImport>();
            post_import_script->set_script(scr.get_ref_ptr());
            if (!post_import_script->get_script_instance()) {
                EditorNode::add_io_error(TTR("Invalid/broken script for post-import (check console):") +
                                         se_string_view(" " + post_import_script_path));
                post_import_script.unref();
                return ERR_CANT_CREATE;
            }
        }
    }

    if (post_import_script) {
        post_import_script->init(base_path, p_source_file);
        scene = post_import_script->post_import(scene);
        if (!scene) {
            EditorNode::add_io_error(
                    TTR("Error running post-import script:") + " " + se_string_view(post_import_script_path));
            return err;
        }
    }

    progress.step(TTR("Saving..."), 104);

    if (external_scenes) {
        // save sub-scenes as instances!
        for (int i = 0; i < scene->get_child_count(); i++) {
            Node *child = scene->get_child(i);
            if (child->get_owner() != scene)
                continue; // not a real child probably created by scene type (ig, a scrollbar)
            _replace_owner(child, scene, child);

            se_string cn = StringUtils::replace(
                    StringUtils::replace(StringUtils::strip_edges(child->get_name()), '.', '_'), ':', '_');
            if (cn.empty()) {
                cn = "ChildNode" + itos(i);
            }
            se_string path = PathUtils::plus_file(base_path, cn + ".scn");
            child->set_filename(path);

            Ref<PackedScene> packer(make_ref_counted<PackedScene>());
            packer->pack(child);
            err = ResourceSaver::save(path, packer); // do not take over, let the changed files reload themselves
            ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save scene to file '" + path + "'.")
        }
    }

    Ref<PackedScene> packer(make_ref_counted<PackedScene>());
    packer->pack(scene);
    print_verbose("Saving scene to: " + se_string(p_save_path) + ".scn");
    err = ResourceSaver::save(
            se_string(p_save_path) + ".scn", packer); // do not take over, let the changed files reload themselves
    ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save scene to file '" + se_string(p_save_path) + ".scn'.")

    memdelete(scene);

    // this is not the time to reimport, wait until import process is done, import file is saved, etc.
    // EditorNode::get_singleton()->reload_scene(p_source_file);

    return OK;
}

ResourceImporterScene *ResourceImporterScene::singleton = nullptr;

ResourceImporterScene::ResourceImporterScene() {
    singleton = this;
}
///////////////////////////////////////

uint32_t EditorSceneImporterESCN::get_import_flags() const {
    return IMPORT_SCENE;
}
void EditorSceneImporterESCN::get_extensions(PODVector<se_string> &r_extensions) const {
    r_extensions.push_back("escn");
}
Node *EditorSceneImporterESCN::import_scene(se_string_view p_path, uint32_t /*p_flags*/, int /*p_bake_fps*/,
        PODVector<se_string> * /*r_missing_deps*/, Error * /*r_err*/) {

    Error error;
    Ref<PackedScene> ps =
            dynamic_ref_cast<PackedScene>(ResourceFormatLoaderText::singleton->load(p_path, p_path, &error));
    ERR_FAIL_COND_V_MSG(not ps, nullptr, "Cannot load scene as text resource from path '" + se_string(p_path) + "'.")

    Node *scene = ps->instance();
    ERR_FAIL_COND_V(!scene, nullptr)

    return scene;
}
Ref<Animation> EditorSceneImporterESCN::import_animation(se_string_view p_path, uint32_t p_flags, int p_bake_fps) {
    ERR_FAIL_V(Ref<Animation>())
}
