#pragma once
#include "core/forward_decls.h"

class ScriptingGlueInterface {

public:
    //! Registers all bound methods in the scripting engine
    virtual bool register_methods() = 0;
    virtual ~ScriptingGlueInterface() = default;
};

