#pragma once

#include "scene/3d/visual_instance_3d.h"
#include "scene/resources/scene_library.h"

/**
 * @brief A placeholder for an instance of a packed scene stored in scene library
 * when it enters an active tree, it will replace itself by instantiating it's target packed scene
 * @see SceneLibrary
 */
class GODOT_EXPORT LibraryEntryInstance : public Node3D {
    GDCLASS(LibraryEntryInstance, Node3D)
    OBJ_CATEGORY("3D")

    String lib_name;
    String entry_name;
    Ref<SceneLibrary> resolved_library;
    bool instantiation_pending = false;

protected:
    void _notification(int p_what);
    // bind helpers
    static void _bind_methods();
    bool instantiate();

public:
    void set_library(const Ref<SceneLibrary> &lib);
    Ref<SceneLibrary> get_library() const { return resolved_library; }

    void set_library_path(const String &lib);
    const String &get_library_path() const { return lib_name; }

    void set_entry(StringView name);
    const String &get_entry() const { return entry_name; }

    Node *instantiate_resolved();

    LibraryEntryInstance();
    ~LibraryEntryInstance() override;
};
