#include "core/engine_tooling.h"

namespace EngineTooling {
#ifdef TOOLS_ENABLED
static bool editor_hint = false;

void set_editor_hint(bool p_enabled) {
    editor_hint = p_enabled;
}
bool is_editor_hint() {
    return editor_hint;
}

#else
void set_editor_hint(bool p_enabled) {
}
bool is_editor_hint() const {
    return false;
}

#endif
} // namespace EngineTooling
