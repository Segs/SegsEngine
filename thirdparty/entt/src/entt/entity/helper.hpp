#ifndef ENTT_ENTITY_HELPER_HPP
#define ENTT_ENTITY_HELPER_HPP

#include "EASTL/type_traits.h"
#include "../config/config.h"
#include "../core/type_traits.hpp"
#include "../signal/delegate.hpp"
#include "registry.hpp"
#include "fwd.hpp"


namespace entt {


/**
 * @brief Converts a registry to a view.
 * @tparam Const Constness of the accepted registry.
 * @tparam Entity A valid entity type (see entt_traits for more details).
 */
template<bool Const, typename Entity, typename Allocator>
struct as_view {
    /*! @brief Type of registry to convert. */
    using registry_type = eastl::conditional_t<Const, const entt::basic_registry<Entity,Allocator>, entt::basic_registry<Entity,Allocator>>;

    /**
     * @brief Constructs a converter for a given registry.
     * @param source A valid reference to a registry.
     */
    as_view(registry_type &source) ENTT_NOEXCEPT: reg{source} {}

    /**
     * @brief Conversion function from a registry to a view.
     * @tparam Exclude Types of components used to filter the view.
     * @tparam Component Type of components used to construct the view.
     * @return A newly created view.
     */
    template<typename Exclude, typename... Component>
    operator entt::basic_view<Entity, Allocator, Exclude, Component...>() const {
        return reg.template view<Component...>(Exclude{});
    }

private:
    registry_type &reg;
};


/**
 * @brief Deduction guide.
 *
 * It allows to deduce the constness of a registry directly from the instance
 * provided to the constructor.
 *
 * @tparam Entity A valid entity type (see entt_traits for more details).
 */
template<typename Entity, typename Allocator>
as_view(basic_registry<Entity,Allocator> &) ENTT_NOEXCEPT -> as_view<false, Entity,Allocator>;


/*! @copydoc as_view */
template<typename Entity, typename Allocator>
as_view(const basic_registry<Entity,Allocator> &) ENTT_NOEXCEPT -> as_view<true, Entity,Allocator>;


/**
 * @brief Converts a registry to a group.
 * @tparam Const Constness of the accepted registry.
 * @tparam Entity A valid entity type (see entt_traits for more details).
 */
template<bool Const, typename Entity, typename Allocator>
struct as_group {
    /*! @brief Type of registry to convert. */
    using registry_type = eastl::conditional_t<Const, const entt::basic_registry<Entity,Allocator>, entt::basic_registry<Entity,Allocator>>;

    /**
     * @brief Constructs a converter for a given registry.
     * @param source A valid reference to a registry.
     */
    as_group(registry_type &source) ENTT_NOEXCEPT: reg{source} {}

    /**
     * @brief Conversion function from a registry to a group.
     * @tparam Exclude Types of components used to filter the group.
     * @tparam Get Types of components observed by the group.
     * @tparam Owned Types of components owned by the group.
     * @return A newly created group.
     */
    template<typename Exclude, typename Get, typename... Owned>
    operator entt::basic_group<Entity,Allocator, Exclude, Get, Owned...>() const {
        return reg.template group<Owned...>(Get{}, Exclude{});
    }

private:
    registry_type &reg;
};


/**
 * @brief Deduction guide.
 *
 * It allows to deduce the constness of a registry directly from the instance
 * provided to the constructor.
 *
 * @tparam Entity A valid entity type (see entt_traits for more details).
 */
template<typename Entity,typename Allocator>
as_group(basic_registry<Entity,Allocator> &) ENTT_NOEXCEPT -> as_group<false, Entity,Allocator>;


/*! @copydoc as_group */
template<typename Entity,typename Allocator>
as_group(const basic_registry<Entity,Allocator> &) ENTT_NOEXCEPT -> as_group<true, Entity,Allocator>;


/**
 * @brief Helper to create a listener that directly invokes a member function.
 * @tparam Member Member function to invoke on a component of the given type.
 * @tparam Entity A valid entity type (see entt_traits for more details).
 * @param reg A registry that contains the given entity and its components.
 * @param entt Entity from which to get the component.
 */
template<auto Member, typename Entity = entity>
void invoke(basic_registry<Entity> &reg, const Entity entt) {
    static_assert(eastl::is_member_function_pointer_v<decltype(Member)>);
    delegate<void(basic_registry<Entity> &, const Entity)> func;
    func.template connect<Member>(reg.template get<member_class_t<decltype(Member)>>(entt));
    func(reg, entt);
}


}


#endif
