#ifndef ENTT_CORE_IDENT_HPP
#define ENTT_CORE_IDENT_HPP


#include "EASTL/tuple.h"
#include "EASTL/utility.h"
#include "EASTL/type_traits.h"
#include "../config/config.h"


namespace entt {


/**
 * @brief Types identifiers.
 *
 * Variable template used to generate identifiers at compile-time for the given
 * types. Use the `get` member function to know what's the identifier associated
 * to the specific type.
 *
 * @note
 * Identifiers are constant expression and can be used in any context where such
 * an expression is required. As an example:
 * @code{.cpp}
 * using id = entt::identifier<a_type, another_type>;
 *
 * switch(a_type_identifier) {
 * case id::type<a_type>:
 *     // ...
 *     break;
 * case id::type<another_type>:
 *     // ...
 *     break;
 * default:
 *     // ...
 * }
 * @endcode
 *
 * @tparam Types List of types for which to generate identifiers.
 */
template<typename... Types>
class identifier {
    using tuple_type = eastl::tuple<std::decay_t<Types>...>;

    template<typename Type, std::size_t... Indexes>
    static constexpr ENTT_ID_TYPE get(eastl::index_sequence<Indexes...>) ENTT_NOEXCEPT {
        static_assert(eastl::disjunction_v<eastl::is_same<Type, Types>...>);
        return (0 + ... + (eastl::is_same_v<Type, eastl::tuple_element_t<Indexes, tuple_type>> ? ENTT_ID_TYPE(Indexes) : ENTT_ID_TYPE{}));
    }

public:
    /*! @brief Unsigned integer type. */
    using identifier_type = ENTT_ID_TYPE;

    /*! @brief Statically generated unique identifier for the given type. */
    template<typename Type>
    static constexpr identifier_type type = get<eastl::decay_t<Type>>(eastl::index_sequence_for<Types...>{});
};


}


#endif // ENTT_CORE_IDENT_HPP
