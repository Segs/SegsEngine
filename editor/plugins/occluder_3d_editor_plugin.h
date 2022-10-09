#pragma once

#include <editor/editor_plugin.h>

class EditorNode;
class Occluder;

///////////////////////

class OccluderEditorPlugin : public EditorPlugin {
    GDCLASS(OccluderEditorPlugin, EditorPlugin);

    Occluder *_occluder;
    ToolButton *button_center;
    EditorNode *editor;
    UndoRedo *undo_redo;

    void _center();

protected:
    static void _bind_methods() {}

public:
    virtual StringView get_name() const override { return "Occluder"; }
    bool has_main_screen() const override { return false; }
    void edit(Object *p_object) override;
    bool handles(Object *p_object) const override;
    void make_visible(bool p_visible) override;


    OccluderEditorPlugin(EditorNode *p_node);
    ~OccluderEditorPlugin();
};
