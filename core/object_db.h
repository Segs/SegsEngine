#pragma once

#include "core/hash_map.h"
#include "core/hashfuncs.h"
#include "core/os/rw_lock.h"

class Object;
using ObjectID = uint64_t;

template<>
struct Hasher<Object *> {

    uint32_t operator()(const Object *p_obj) const {
        union {
            const Object *p;
            unsigned long i;
        } u;
        u.p = p_obj;
        return Hasher<uint64_t>()((uint64_t)u.i);
    }
};

class ObjectDB {

    static HashMap<ObjectID, Object *> instances;
    static HashMap<Object *, ObjectID, Hasher<Object *>> instance_checks;
    static ObjectID instance_counter;
    static RWLock *rw_lock;

    static void cleanup();
    static ObjectID add_instance(Object *p_object);
    static void remove_instance(Object *p_object);
    static void setup();

    friend class Object;
    friend void unregister_core_types();
    friend void register_core_types();
public:
    using DebugFunc = void (*)(Object *);

    GODOT_EXPORT static Object *get_instance(ObjectID p_instance_id);
    GODOT_EXPORT static void debug_objects(DebugFunc p_func);
    GODOT_EXPORT static int get_object_count();

    _FORCE_INLINE_ static bool instance_validate(Object *p_ptr) {
        rw_lock->read_lock();

        bool exists = instance_checks.contains(p_ptr);

        rw_lock->read_unlock();

        return exists;
    }
};
