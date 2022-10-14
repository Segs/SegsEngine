#pragma once

#include <editor/editor_plugin.h>

class EditorNode;
class RoomManager;
class Room;
class Portal;

class RoomManagerEditorPlugin : public EditorPlugin
{
    GDCLASS(RoomManagerEditorPlugin, EditorPlugin);

    RoomManager *_room_manager;

    ToolButton *button_flip_portals;
    EditorNode *editor;

    void _flip_portals();

protected:
    static void _bind_methods() {}

public:
    virtual StringView get_name() const override { return "RoomManager"; }
    bool has_main_screen() const override { return false; }
    void edit(Object *p_object) override;
    bool handles(Object *p_object) const override;
    void make_visible(bool p_visible) override;

    RoomManagerEditorPlugin(EditorNode *p_node);
    ~RoomManagerEditorPlugin();
};

///////////////////////

class RoomEditorPlugin : public EditorPlugin
{
    GDCLASS(RoomEditorPlugin,EditorPlugin);

    Room *_room;
    ToolButton *button_generate;
    EditorNode *editor;
    UndoRedo *undo_redo;

    void _generate_points();

protected:
    static void _bind_methods() {}

public:
    virtual StringView get_name() const override { return "Room"; }
    bool has_main_screen() const override { return false; }
    void edit(Object *p_object) override;
    bool handles(Object *p_object) const override;
    void make_visible(bool p_visible) override;

    RoomEditorPlugin(EditorNode *p_node);
    ~RoomEditorPlugin();
};

///////////////////////

class PortalEditorPlugin : public EditorPlugin {
    GDCLASS(PortalEditorPlugin, EditorPlugin);

    Portal *_portal;
    ToolButton *button_flip;
    EditorNode *editor;

    void _flip_portal();

protected:
    static void _bind_methods() {}

public:
    virtual StringView get_name() const override { return "Portal"; }
    bool has_main_screen() const override { return false; }
    void edit(Object *p_object) override;
    bool handles(Object *p_object) const override;
    void make_visible(bool p_visible) override;

    PortalEditorPlugin(EditorNode *p_node);
    ~PortalEditorPlugin();
};
