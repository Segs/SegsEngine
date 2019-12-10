#include "service_interfaces/EditorServiceInterface.h"

#include "editor/editor_node.h"

class EditorServiceInterfaceImpl : public EditorServiceInterface
{
public:
    void reportError(const StringName &msg) override {
        EditorNode::add_io_error(msg);
    }

};
EditorServiceInterface *getEditorInterface() {
    static EditorServiceInterfaceImpl s_instance;
    return &s_instance;
}
