#include "object_db.h"

#include "object.h"
#include "core/error_list.h"
#include "core/error_macros.h"
#include "core/hash_map.h"
#include "core/object_id.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "core/string.h"
#include "core/string_formatter.h"
#include "core/hash_map.h"
#include "core/os/rw_lock.h"

namespace  {
HashMap<ObjectID, Object *> *s_instances=nullptr;
HashMap<Object *, ObjectID, Hasher<Object *>> instance_checks;
uint64_t s_instance_counter;
RWLock s_odb_rw_lock;

}

ObjectID ObjectDB::add_instance(Object *p_object) {

    ERR_FAIL_COND_V(p_object->get_instance_id().is_valid(), ObjectID());

    RWLockWrite guard(s_odb_rw_lock);
    ObjectID instance_id(++s_instance_counter);
    (*s_instances)[instance_id] = p_object;
    instance_checks[p_object] = instance_id;

    return instance_id;
}

void ObjectDB::remove_instance(Object *p_object) {

    RWLockWrite guard(s_odb_rw_lock);

    (*s_instances).erase(p_object->get_instance_id());
    instance_checks.erase(p_object);
}
Object *ObjectDB::get_instance(ObjectID p_instance_id) {
    RWLockRead guard(s_odb_rw_lock);

    auto iter= (*s_instances).find(p_instance_id);
    Object *obj = iter!=(*s_instances).end() ? iter->second : nullptr;

    return obj;
}

void ObjectDB::debug_objects(DebugFunc p_func) {

    RWLockRead guard(s_odb_rw_lock);

    for(const auto &e : (*s_instances)) {

        p_func(e.second);
    }
}

int ObjectDB::get_object_count() {
    RWLockRead guard(s_odb_rw_lock);
    int count = (*s_instances).size();

    return count;
}

bool ObjectDB::instance_validate(Object *p_ptr) {
    RWLockRead guard(s_odb_rw_lock);

    bool exists = instance_checks.contains(p_ptr);

    return exists;
}

void ObjectDB::setup() {

    s_instances = new HashMap<ObjectID, Object *>();
}

void ObjectDB::cleanup() {

    RWLockWrite guard(s_odb_rw_lock);
    if (!(*s_instances).empty()) {

        WARN_PRINT("ObjectDB Instances still exist!");
        if (OS::get_singleton()->is_stdout_verbose()) {
            for (const auto &e : (*s_instances)) {
                String node_name;
#ifdef DEBUG_ENABLED
                const char *name = e.second->get_dbg_name();
                if (name) {
                    node_name = FormatVE(" - %s name: %s",e.second->get_class_name().asCString(),name);
                }
#endif
                print_line(FormatVE("Leaked instance: %s:%p:%s", e.second->get_class(), e.second,node_name.c_str()));
            }
        }
    }
    s_instances->clear();
    instance_checks.clear();
    delete s_instances;
}
