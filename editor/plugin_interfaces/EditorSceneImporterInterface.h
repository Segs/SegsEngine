#pragma once
#include "core/forward_decls.h"
#include <cstdint>

class Animation;
class Node;
enum Error : int;

class EditorSceneImporterInterface {
public:
    enum ImportFlags {
        IMPORT_SCENE = 1,
        IMPORT_ANIMATION = 2,
        IMPORT_ANIMATION_DETECT_LOOP = 4,
        IMPORT_ANIMATION_OPTIMIZE = 8,
        IMPORT_ANIMATION_FORCE_ALL_TRACKS_IN_ALL_CLIPS = 16,
        IMPORT_ANIMATION_KEEP_VALUE_TRACKS = 32,
        IMPORT_GENERATE_TANGENT_ARRAYS = 256,
        IMPORT_FAIL_ON_MISSING_DEPENDENCIES = 512,
        IMPORT_MATERIALS_IN_INSTANCES = 1024,
        IMPORT_USE_COMPRESSION = 2048,
        IMPORT_USE_NAMED_SKIN_BINDS = 4096,

    };
    virtual uint32_t get_import_flags() const=0;
    virtual void get_extensions(Vector<String> &p_extensions) const = 0;
    virtual Node *import_scene(StringView p_path, uint32_t p_flags, int p_bake_fps, Vector<String> *r_missing_deps, Error *r_err = nullptr)=0;
    virtual Ref<Animation> import_animation(StringView p_path, uint32_t p_flags, int p_bake_fps)=0;

    virtual ~EditorSceneImporterInterface() = default;
};


class EditorSceneExporterInterface {
//    friend class ImageSaver;
public:
    virtual bool can_save(const String &extension)=0; // support for multi-format plugins
    virtual void get_extensions(Vector<String> *p_extensions) const = 0;

    virtual ~EditorSceneExporterInterface() = default;
};
