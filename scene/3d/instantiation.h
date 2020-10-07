#pragma once

#include "scene/3d/visual_instance_3d.h"
#include "scene/resources/scene_library.h"

class GODOT_EXPORT LibraryEntryInstance : public Node3D {
    GDCLASS(LibraryEntryInstance, Node3D)
    OBJ_CATEGORY("3D")

    String lib_name;
    String entry_name;
    Ref<SceneLibrary> resolved_library;
    Node3D *instantiated_child=nullptr;
protected:
    void _notification(int p_what);
    // bind helpers
    static void _bind_methods();
    void update_instance();

public:
    void set_library(const Ref<SceneLibrary> &lib);
    Ref<SceneLibrary> get_library() const { return resolved_library; }

    void set_library_path(const String &lib);
    const String &get_library_path() const { return lib_name; }

    void set_entry(StringView name);
    const String &get_entry() const { return entry_name; }

    LibraryEntryInstance();
    ~LibraryEntryInstance() override;
};
