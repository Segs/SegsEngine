#pragma once

#include "core/forward_decls.h"
enum PackedGenEditState : unsigned char;

namespace PackedSceneTooling {
    GODOT_EXPORT bool can_instance_state(PackedGenEditState p_edit_state);
}
