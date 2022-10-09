#pragma once

#include "core/reflection_macros.h"
#include "core/hashfuncs.h"

class Object;

namespace ObjectDB {
GODOT_EXPORT bool is_valid_object(Object *);
GODOT_EXPORT void cleanup();

    }


