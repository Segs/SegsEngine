#pragma once

#include "core/hash_map.h"
#include "core/hashfuncs.h"
#include "core/os/rw_lock.h"
#include "core/object_id.h"

class Object;

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

    RWLock *rw_lock=nullptr;
    HashMap<Object *, ObjectID, Hasher<Object *>> instance_checks;

    void cleanup();
    void setup();

    ObjectID add_instance(Object *p_object);
    void remove_instance(Object *p_object);

    friend class Object;
    friend void unregister_core_types();
    friend void register_core_types();
    friend GODOT_EXPORT ObjectDB &gObjectDB();
    GODOT_EXPORT static ObjectDB *s_instance;
protected:
    ObjectDB()=default;
    ~ObjectDB()=default;
    ObjectDB(const ObjectDB&)=delete;

public:
    using DebugFunc = void (*)(Object *);

    GODOT_EXPORT static Object *get_instance(ObjectID p_instance_id);
    GODOT_EXPORT void debug_objects(DebugFunc p_func);
    GODOT_EXPORT int get_object_count();

    _FORCE_INLINE_ bool instance_validate(Object *p_ptr) {
        rw_lock->read_lock();

        bool exists = instance_checks.contains(p_ptr);

        rw_lock->read_unlock();

        return exists;
    }

};
inline ObjectDB &gObjectDB()  { return *ObjectDB::s_instance; }
