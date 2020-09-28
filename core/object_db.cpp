#include "object_db.h"

#include "object.h"
#include "core/hash_map.h"
#include "core/error_macros.h"
#include "core/error_list.h"
#include "core/os/os.h"
#include "core/string.h"
#include "core/string_formatter.h"
#include "core/print_string.h"

namespace  {
HashMap<ObjectID, Object *> *s_instances=nullptr;
uint64_t s_instance_counter;

}

ObjectID ObjectDB::add_instance(Object *p_object) {

    ERR_FAIL_COND_V(p_object->get_instance_id().is_valid(), ObjectID());

    rw_lock->write_lock();
    ObjectID instance_id(++s_instance_counter);
    (*s_instances)[instance_id] = p_object;
    instance_checks[p_object] = instance_id;

    rw_lock->write_unlock();

    return instance_id;
}

void ObjectDB::remove_instance(Object *p_object) {

    rw_lock->write_lock();

    (*s_instances).erase(p_object->get_instance_id());
    instance_checks.erase(p_object);

    rw_lock->write_unlock();
}
Object *ObjectDB::get_instance(ObjectID p_instance_id) {

    ObjectDB &self(gObjectDB());
    self.rw_lock->read_lock();
    auto iter= (*s_instances).find(p_instance_id);
    Object *obj = iter!=(*s_instances).end() ? iter->second : nullptr;
    self.rw_lock->read_unlock();

    return obj;
}

void ObjectDB::debug_objects(DebugFunc p_func) {

    rw_lock->read_lock();

    for(const auto &e : (*s_instances)) {

        p_func(e.second);
    }

    rw_lock->read_unlock();
}

int ObjectDB::get_object_count() {

    rw_lock->read_lock();
    int count = (*s_instances).size();
    rw_lock->read_unlock();

    return count;
}

void ObjectDB::setup() {

    rw_lock = RWLock::create();
    s_instances = new HashMap<ObjectID, Object *>();
}

void ObjectDB::cleanup() {

    rw_lock->write_lock();
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
                print_line(FormatVE("Leaked instance: %s:%zu%s", e.second->get_class(), e.second,node_name.c_str()));
            }
        }
    }
    (*s_instances).clear();
    instance_checks.clear();
    rw_lock->write_unlock();
    delete s_instances;
    memdelete(rw_lock);
}
/// \note this function leaks ObjectDB
ObjectDB *ObjectDB::s_instance = new ObjectDB();
