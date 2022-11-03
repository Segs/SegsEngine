#pragma once

#include "core/forward_decls.h"

class Resource;
namespace ResourceTooling {
GODOT_EXPORT void set_import_path(Resource *, StringView path);
GODOT_EXPORT const String &get_import_path(const Resource *r);
// helps keep IDs same number when loading/saving scenes. -1 clears ID and it Returns -1 when no id stored
GODOT_EXPORT void set_id_for_path(const Resource *r, StringView p_path, int p_id);
GODOT_EXPORT int get_id_for_path(const Resource *r, StringView p_path);

GODOT_EXPORT void set_last_modified_time(const Resource *r, uint64_t p_time);
GODOT_EXPORT uint64_t get_last_modified_time(const Resource *r);
GODOT_EXPORT void set_last_modified_time_from_another(const Resource *r, const Resource *other);

GODOT_EXPORT void set_import_last_modified_time(const Resource *r, uint64_t p_time);
GODOT_EXPORT uint64_t get_import_last_modified_time(const Resource *r);
}
 