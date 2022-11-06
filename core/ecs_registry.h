#pragma once

#include "core/os/memory.h"
#include "core/os/mutex.h"
#include "core/engine_entities.h"

#include "entt/entity/registry.hpp"
#include <thread>

//! Used to mark the meta property that provides an user visible name
constexpr entt::hashed_string g_display_name_prop_key("DisplayName");
//! A key for a meta-property that provides a property group index
constexpr entt::hashed_string g_group_index_prop_key("GroupIndex");
//! A key for a meta-property that signifies that a given data field is a property
constexpr entt::hashed_string g_property_prop_key("Property");
//! A key for a meta-property that signifies that a given type properties are grouped
constexpr entt::hashed_string g_property_groups("PropertyGroups");
//! A key for a meta-property that signifies that a given data field has a simple PropertyRange defined
constexpr entt::hashed_string g_property_range_prop_key("Range");
//! A key for a meta-property that signifies that a given value is the propertie's default value
constexpr entt::hashed_string g_property_default_prop_key("Default");
// TODO: add the option to set a default value for a property with an optional reset operation

#define ENTT_TYPE_REFL(type_name) entt::meta<type_name>().type(entt::hashed_string(#type_name))

#define ENTT_ENUM_REFL(enumn, field, display_name)                                                                     \
    type_meta.data<enumn::field>(entt::hashed_string(#enumn "::" #field))                                                      \
            .prop(g_display_name_prop_key, StringName(display_name))

#define ENTT_METHOD_REFL(name) type_meta.func<&ReflectedType::name>(entt::hashed_string(#name))
#define ENTT_FUNCTION_REFL(name) type_meta.func<name>(entt::hashed_string(#name))

#define ENTT_PROPERTY_REFL(setter,getter,name)                                                              \
    type_meta.data<&ReflectedType::setter, &ReflectedType::getter>(name##_hs).prop(g_property_prop_key)

#define ENTT_FUNCTION_ACCESSORS(setter, getter) setter, getter
#define ENTT_MEMBER_ACCESSORS(setter, getter) &ReflectedType::setter, &ReflectedType::getter
#define ENTT_PROPERTY_EX_REFL(accessors, name, ...)                                                    \
    type_meta.data<accessors>(name##_hs).props(                        \
            eastl::make_pair(g_property_prop_key, true), __VA_ARGS__)

#define ENTT_DEFAULT_PROPERTY_VALUE(val) eastl::make_pair(g_property_default_prop_key,val)

// used as one of ENTT_PROPERTY_EX_REFL additional arguments, it marks the gi
#define ENTT_GROUP_PROPERTY_IDX(group_idx) eastl::make_pair(g_group_index_prop_key,group_idx)
// used as one of ENTT_PROPERTY_EX_REFL additional arguments, it defines a valid range for the property
#define ENTT_PROPERTY_RANGE(...) std::make_pair(g_property_range_prop_key, PropertyRange{ __VA_ARGS__ })

#define ENTT_START_REFL(type_name) using ReflectedType = type_name;\
    auto type_meta = ENTT_TYPE_REFL(type_name)

#define ENTT_END_REFL()

#define ENTT_ADD_EDITOR_FUNCS()\
    type_meta.func<entt::overload<ReflectedType &(GameEntity)>(&ECS_Registry<GameEntity,true>::RegistryType::get<ReflectedType>), entt::as_ref_t>("get"_hs)

/** \note Reflected components have optional properties that can be set on their meta_type :
 *  "PropertyGroups" - contains a Span<const PropertyGroupInfo> describing all available property groups
 *
 *  \note Reflected component data fields can have optional properties on them
 *
*/

struct PropertyGroupInfo {
    const char *m_display_name;
    const char *m_prefix;
};

struct PropertyRange {
    enum RangeBounds : uint8_t {
        CLOSED_RANGE=0,
        OR_GREATER,
        OR_SMALLER
    };
    float min_value;
    float max_value;
    float step;
    RangeBounds flags = CLOSED_RANGE;
};

class ECS_Registry_Base {
protected:
    std::thread::id creation_thread;
public:
    void initialize() {
        // only one initialize call can be made.
        assert(creation_thread==std::thread::id());
        // registry is now bound to the thread.
        creation_thread = std::this_thread::get_id();
    }
};
template<bool UseLocking>
class RegistryLock;

// Dummy lock for single-thread and job-graph based registries
// For example when using job-graph resource dependency resolver there's no need to synchronize access to the
// ecs repository.
template<>
class RegistryLock<false> {
    constexpr void lock_registry() {}
    constexpr void unlock_registry() {}
};

// Mutex based registry access guard.
template<>
class RegistryLock<true> {
    Mutex registry_w_lock;
public:
    void lock_registry() { registry_w_lock.lock(); }
    void unlock_registry() { registry_w_lock.unlock(); }
};
template<class Entity, bool multi_threaded>
class ECS_Registry : public ECS_Registry_Base, public RegistryLock<multi_threaded> {
public:
    using RegistryType = entt::basic_registry<Entity,wrap_allocator>;
    wrap_allocator alloc_instance {};
    entt::basic_registry<Entity,wrap_allocator> registry {alloc_instance};
    bool is_registry_access_valid_from_this_thread() const {
        if constexpr(multi_threaded) {
            return true; // access is always ok, but writing should be guarded by the
        }
        else {
            return creation_thread == std::this_thread::get_id();
        }
    }

    // this function wraps registry.create for a single reason:setting breakpoint on specific entity's creation.
    Entity create() {
        Entity res=registry.create();
//        if(entt::to_integral(res)==0x9) {
//            volatile int val=0; // breakpoint location
//        }
        return res;
    }
    template<typename Component>
    Entity create() {
        auto res = create();
        registry.template emplace<Component>(res);
        return res;
    }
    bool valid(Entity e) const {
        return registry.valid(e);
    }
    template<typename... Component>
    [[nodiscard]] auto try_get([[maybe_unused]] const Entity entity) {
        return registry.template try_get<Component...>(entity);
    }
    template<typename... Component>
    [[nodiscard]] auto get_or_null([[maybe_unused]] const Entity entity) {
        return entity!=entt::null ? registry.template try_get<Component...>(entity) : nullptr;
    }
};

extern ECS_Registry<GameEntity,true> game_object_registry;
