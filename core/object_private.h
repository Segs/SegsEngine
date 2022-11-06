#pragma once
#include "core/hash_map.h"
#include "core/method_info.h"
#include "core/object_tooling.h"
#include "core/object.h"
#include "EASTL/list.h"
#include "EASTL/vector_map.h"

struct Object::SignalData  {
    struct Slot {
        int reference_count = 0;
        Connection conn;
        List<Connection>::iterator cE;
    };

    MethodInfo user;
    eastl::vector_map<Callable, Slot> slot_map;
    //VMap<Callable, Slot> slot_map;
};

struct Object::ObjectPrivate {
    IObjectTooling *m_tooling;
    HashMap<StringName, SignalData> signal_map;
    List<Connection> connections;

#ifdef DEBUG_ENABLED
    SafeRefCount _lock_index;
#endif

    ObjectPrivate(Object *self)
    {
#ifdef DEBUG_ENABLED
        _lock_index.init(1);
#endif
        m_tooling = create_tooling_for(self);
    }
    ~ObjectPrivate() {
        //TODO: use range-based-for + signal_map.clear() afterwards ?
        while(!signal_map.empty()) {
            SignalData *s = &signal_map.begin()->second;

            //brute force disconnect for performance
            const auto *slot_list = s->slot_map.data();
            int slot_count = s->slot_map.size();
            for (int i = 0; i < slot_count; i++) {
                slot_list[i].second.conn.callable.get_object()->private_data->connections.erase(slot_list[i].second.cE);
            }

            signal_map.erase(signal_map.begin());
        }

        //signals from nodes that connect to this node
        while (connections.size()) {
            Connection c = connections.front();
            c.signal.get_object()->_disconnect(c.signal.get_name(), c.callable, true);
        }
        relase_tooling(m_tooling);
        m_tooling = nullptr;
    }
    IObjectTooling *get_tooling() {
        return m_tooling;
    }
};

