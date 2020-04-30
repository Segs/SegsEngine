#include "object_db.h"

#include "object.h"
#include "core/hash_map.h"
#include "core/error_macros.h"
#include "core/error_list.h"
#include "core/os/os.h"
#include "core/string.h"
#include "core/string_formatter.h"
#include "core/print_string.h"

HashMap<ObjectID, Object *> ObjectDB::instances;
ObjectID ObjectDB::instance_counter = 1;
HashMap<Object *, ObjectID, Hasher<Object *>> ObjectDB::instance_checks;
ObjectID ObjectDB::add_instance(Object *p_object) {

    ERR_FAIL_COND_V(p_object->get_instance_id() != 0, 0);

    rw_lock->write_lock();
    ObjectID instance_id = ++instance_counter;
    instances[instance_id] = p_object;
    instance_checks[p_object] = instance_id;

    rw_lock->write_unlock();

    return instance_id;
}

void ObjectDB::remove_instance(Object *p_object) {

    rw_lock->write_lock();

    instances.erase(p_object->get_instance_id());
    instance_checks.erase(p_object);

    rw_lock->write_unlock();
}
Object *ObjectDB::get_instance(ObjectID p_instance_id) {

    rw_lock->read_lock();
    auto iter= instances.find(p_instance_id);
    Object *obj = iter!=instances.end() ? iter->second : nullptr;
    rw_lock->read_unlock();

    return obj;
}

void ObjectDB::debug_objects(DebugFunc p_func) {

    rw_lock->read_lock();

    for(const auto &e : instances) {

        p_func(e.second);
    }

    rw_lock->read_unlock();
}

int ObjectDB::get_object_count() {

    rw_lock->read_lock();
    int count = instances.size();
    rw_lock->read_unlock();

    return count;
}

RWLock *ObjectDB::rw_lock = nullptr;

void ObjectDB::setup() {

    rw_lock = RWLock::create();
}

void ObjectDB::cleanup() {

    rw_lock->write_lock();
    if (!instances.empty()) {

        WARN_PRINT("ObjectDB Instances still exist!");
        if (OS::get_singleton()->is_stdout_verbose()) {
            for (const auto &e : instances) {
                String node_name;
#ifdef DEBUG_ENABLED
                const char *name = e.second->get_dbg_name();
                if (name) {
                    node_name = FormatVE(" - %s name: %s",e.second->get_class_name().asCString(),name);
                }
#endif
                print_line(FormatVE("Leaked instance: %s:%zu%s", e.second->get_class(), e.second,node_name.c_str()));
            }
        }
    }
    instances.clear();
    instance_checks.clear();
    rw_lock->write_unlock();
    memdelete(rw_lock);
}
