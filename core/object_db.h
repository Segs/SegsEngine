#pragma once

#include "core/hashfuncs.h"

class Object;
class ObjectID;

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
    friend class Object;
    friend void unregister_core_types();
    friend void register_core_types();

    static void cleanup();
    static void setup();

    static ObjectID add_instance(Object *p_object);
    static void remove_instance(Object *p_object);

protected:
    ObjectDB()=default;
    ~ObjectDB()=default;
    ObjectDB(const ObjectDB&)=delete;

public:
    using DebugFunc = void (*)(Object *);

    GODOT_EXPORT static Object *get_instance(ObjectID p_instance_id);
    GODOT_EXPORT static void debug_objects(DebugFunc p_func);
    GODOT_EXPORT static int get_object_count();
    GODOT_EXPORT static bool instance_validate(Object *p_ptr);

};

