#pragma once
#include "core/forward_decls.h"
#include "core/string.h"
#include <cstdint>

class Animation;
class Node;
enum Error : int;

class EditorExportPlatform;

class EditorPlatformExportInterface {
public:
    // Checks if the selected platform exporter is available and supported
    virtual bool is_supported() = 0;
    virtual bool create_and_register_exporter(EditorExportPlatform*) = 0;
    virtual EditorExportPlatform* platform()=0;
    virtual void unregister_exporter(EditorExportPlatform*) = 0;

};
