#pragma once

#include "core/godot_export.h"

class FileAccess;
class String;

/** This interface is used by the editor plugins to interact with the engine Editor
 * For now main usages revolve around error/progress reporting
*/
class GODOT_EXPORT EditorServiceInterface
{
public:
    virtual void reportError(const String &msg) = 0;

};
// used internally by the engine to pass service interface to plugins
EditorServiceInterface *getEditorInterface();
