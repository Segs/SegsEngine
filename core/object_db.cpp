#include "object_db.h"

#include "object.h"
#include "core/ecs_registry.h"
#include "core/error_list.h"
#include "core/error_macros.h"
#include "core/hash_map.h"
#include "core/hash_set.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "core/string.h"
#include "core/string_formatter.h"
#include "core/hash_map.h"
#include "core/os/rw_lock.h"

#include "entt/entity/registry.hpp"



namespace ObjectDB {

bool is_valid_object(Object *obj)
{
    return obj && object_for_entity(obj->get_instance_id())==obj;
}

void cleanup() {
    if (game_object_registry.registry.empty()) {
        return; // nothing to report.
}

        WARN_PRINT("ObjectDB Instances still exist!");
        if (OS::get_singleton()->is_stdout_verbose()) {
        game_object_registry.registry.each([](const GameEntity ent) {
                String node_name;
            ObjectLink *link = game_object_registry.registry.try_get<ObjectLink>(ent);
            Object *obj = link ? link->object : nullptr;
            if (obj) {
#ifdef DEBUG_ENABLED
                const char *name = obj->get_dbg_name();
                if (name) {
                    node_name = FormatVE(" - %s name: %s", obj->get_class_name().asCString(), name);
                }
#endif
                print_line(FormatVE("Leaked instance(%x): %s:%p:%s",entt::to_integral(ent), obj->get_class(), obj, node_name.c_str()));
            } else {
                print_line(FormatVE("Leaked non-Object instance: %d", entt::to_integral(ent)));
            }
        });
        }
    }
}
