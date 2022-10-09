#include "packed_scene_tooling.h"

#include "packed_scene.h"
#include "core/resource/resource_tools.h"

namespace PackedSceneTooling {
bool can_instance_state(PackedGenEditState p_edit_state) {
#ifndef TOOLS_ENABLED
    ERR_FAIL_COND_V_MSG(p_edit_state != GEN_EDIT_STATE_DISABLED, false,
            "Edit state is only for editors, does not work without tools compiled.");
#endif
    return true;
}

}

void PackedScene::on_state_changed() {
#ifdef TOOLS_ENABLED
    state->set_last_modified_time(ResourceTooling::get_last_modified_time(this));
#endif
}
