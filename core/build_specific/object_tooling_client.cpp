#include "core/object_tooling.h"

namespace Tooling {
bool class_can_instance_cb(ClassDB_ClassInfo *ti, const StringName &string_name) {
    return true;
}
void add_virtual_method(const StringName &string_name, const MethodInfo &method_info) {}
void generate_phash_translation(PHashTranslation &tgt, const Ref<Translation> &p_from) {}
bool tooling_log() {
    return false;
}
void importer_load(const Ref<Resource> &res, const String &path) {}
bool check_resource_manager_load(StringView p_path) {
    return true;
}
} // namespace Tooling
