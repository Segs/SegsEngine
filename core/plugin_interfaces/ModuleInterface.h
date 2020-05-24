#pragma once
#include "core/forward_decls.h"

class ModuleInterface {

public:
    virtual bool register_module() = 0;
    virtual void unregister_module() = 0;
    virtual ~ModuleInterface() = default;
};
