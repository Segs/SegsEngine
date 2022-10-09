#ifndef ENTT_CONTAINER_FWD_HPP
#define ENTT_CONTAINER_FWD_HPP

#include <functional>
#include <EASTL/memory.h>

namespace entt {

template<
    typename Key, typename Type,
    typename = std::hash<Key>,
    typename = std::equal_to<Key>,
    typename = eastl::allocator> //T<eastl::pair<const Key, Type>>
class dense_map;

template<
    typename Type,
    typename = std::hash<Type>,
    typename = std::equal_to<Type>,
    typename = eastl::allocator> //eastl::allocatorT<Type>
class dense_set;

} // namespace entt

#endif
