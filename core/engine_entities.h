#pragma once

#include "entt/core/fwd.hpp"
#include "entt/entity/entity.hpp"

//enum class GameEntity : entt::id_type {};
//enum class RenderingEntity : entt::id_type {};
struct RenderingEntity {
    using entity_type = std::uint32_t;
    static constexpr auto null = entt::null;

    constexpr RenderingEntity(const RenderingEntity &other) noexcept
        : entt(other.entt) {}
    explicit constexpr RenderingEntity(RenderingEntity &&other) noexcept
            : entt(other.entt) { other.entt = entt::null; }

    constexpr RenderingEntity & operator=(const RenderingEntity &other) noexcept = default;
    constexpr RenderingEntity & operator=(RenderingEntity &&other) noexcept  {
        entt = other.entt;
        other.entt = entt::null;
        return *this;
    }

    constexpr static RenderingEntity c(entity_type value = null) noexcept {
        return RenderingEntity(value);
    }
    explicit constexpr RenderingEntity(entity_type value) noexcept : entt(value) {}

    constexpr RenderingEntity(entt::null_t v) noexcept  : entt(v) {
    }
    explicit constexpr RenderingEntity(entt::tombstone_t v) noexcept : entt{ v } {
    }
    constexpr RenderingEntity() noexcept  : entt(entt::null) {
    }
    constexpr bool is_null() const {
        return entt == entt::null;
    }
    constexpr bool is_valid() const {
        return entt != entt::null;
        // TODO: in debug build, also check if the entity is registry-valid
    }
    constexpr bool operator==(RenderingEntity s) const { return s.entt == entt; }
    constexpr bool operator!=(RenderingEntity s) const { return s.entt != entt; }
    constexpr bool operator==(entt::null_t s) const { return s == entt; }
    constexpr bool operator!=(entt::null_t s) const { return s != entt; }
    constexpr entity_type v() const { return entt; }
private:
    entity_type entt;
};
constexpr RenderingEntity RE(RenderingEntity::entity_type value) noexcept {
    return RenderingEntity(value);
}

template<>
class entt::entt_traits<RenderingEntity> : public entt::internal::entt_traits<RenderingEntity::entity_type> {
    using base_type = internal::entt_traits<RenderingEntity>;

public:
    using value_type = RenderingEntity;
    using entity_type = typename base_type::entity_type;
    using version_type = typename base_type::version_type;
    static constexpr entity_type reserved = base_type::entity_mask | (base_type::version_mask << base_type::entity_shift);
    static constexpr auto page_size = ENTT_SPARSE_PAGE;

    [[nodiscard]] static constexpr entity_type to_integral(const value_type value) ENTT_NOEXCEPT {
        return static_cast<entity_type>(value.v());
    }
    [[nodiscard]] static constexpr entity_type to_entity(const value_type value) ENTT_NOEXCEPT {
        return (to_integral(value) & base_type::entity_mask);
    }
    [[nodiscard]] static constexpr version_type to_version(const value_type value) ENTT_NOEXCEPT {
        return (to_integral(value) >> base_type::entity_shift);
    }
    [[nodiscard]] static constexpr value_type construct(const entity_type entity, const version_type version) ENTT_NOEXCEPT {
        return value_type{(entity & base_type::entity_mask) | (static_cast<entity_type>(version) << base_type::entity_shift)};
    }
    [[nodiscard]] static constexpr value_type combine(const entity_type lhs, const entity_type rhs) ENTT_NOEXCEPT {
        constexpr auto mask = (base_type::version_mask << base_type::entity_shift);
        return value_type{(lhs & base_type::entity_mask) | (rhs & mask)};
    }
    [[nodiscard]] static constexpr value_type combine(const entity_type lhs, const tombstone_t rhs) ENTT_NOEXCEPT {
        constexpr auto mask = (base_type::version_mask << base_type::entity_shift);
        return value_type{(lhs & base_type::entity_mask) | (entity_type(rhs) & mask)};
    }
};


struct GameEntity {
    using entity_type = std::uint32_t;
    static constexpr auto null = entt::null;


    constexpr GameEntity(const GameEntity &other) noexcept
        : entt(other.entt) {}

    constexpr static GameEntity c(entity_type value = null) noexcept {
        return GameEntity(value);
    }
    explicit constexpr GameEntity(entity_type value) noexcept : entt( value ) {}
    constexpr GameEntity(entt::null_t v) noexcept  : entt(v){
    }
    constexpr GameEntity(entt::tombstone_t v) noexcept : entt( v ) {
    }
    constexpr GameEntity() noexcept  : entt(entt::null){
    }
    constexpr bool is_null() const {
        return entt == entt::null;
    }
    constexpr bool is_valid() const {
        return entt != entt::null;
        // TODO: in debug build, also check if the entity is registry-valid
    }
    constexpr bool operator==(GameEntity s) const { return s.entt == entt; }
    constexpr bool operator!=(GameEntity s) const { return s.entt != entt; }
    constexpr bool operator==(entt::null_t s) const { return s == entt; }
    constexpr bool operator!=(entt::null_t s) const { return s != entt; }
    constexpr entity_type v() const { return entt; }
private:
    entity_type entt;
};
constexpr GameEntity GE(GameEntity::entity_type value) noexcept {
    return GameEntity(value);
}

template<>
class entt::entt_traits<GameEntity> : public entt::internal::entt_traits<GameEntity::entity_type> {
    using base_type = internal::entt_traits<GameEntity>;

public:
    using value_type = GameEntity;
    using entity_type = typename base_type::entity_type;
    using version_type = typename base_type::version_type;
    static constexpr entity_type reserved = base_type::entity_mask | (base_type::version_mask << base_type::entity_shift);
    static constexpr auto page_size = ENTT_SPARSE_PAGE;

    [[nodiscard]] static constexpr entity_type to_integral(const value_type value) ENTT_NOEXCEPT {
        return static_cast<entity_type>(value.v());
    }
    [[nodiscard]] static constexpr entity_type to_entity(const value_type value) ENTT_NOEXCEPT {
        return (to_integral(value) & base_type::entity_mask);
    }
    [[nodiscard]] static constexpr version_type to_version(const value_type value) ENTT_NOEXCEPT {
        return (to_integral(value) >> base_type::entity_shift);
    }
    [[nodiscard]] static constexpr value_type construct(const entity_type entity, const version_type version) ENTT_NOEXCEPT {
        return value_type{(entity & base_type::entity_mask) | (static_cast<entity_type>(version) << base_type::entity_shift)};
    }
    [[nodiscard]] static constexpr value_type combine(const entity_type lhs, const entity_type rhs) ENTT_NOEXCEPT {
        constexpr auto mask = (base_type::version_mask << base_type::entity_shift);
        return value_type{(lhs & base_type::entity_mask) | (rhs & mask)};
    }
    [[nodiscard]] static constexpr value_type combine(const entity_type lhs, const tombstone_t rhs) ENTT_NOEXCEPT {
        constexpr auto mask = (base_type::version_mask << base_type::entity_shift);
        return value_type{(lhs & base_type::entity_mask) | (entity_type(rhs) & mask)};
    }
};

namespace eastl {
template<>
struct hash<GameEntity> {
    size_t operator()(GameEntity np) const {
        if (np==entt::null)
            return 0;
        const eastl::hash<entt::id_type> hasher;
        return hasher(entt::to_integral(np));
    }
};
template<>
struct hash<RenderingEntity> {
    size_t operator()(RenderingEntity np) const {
        if (np==entt::null)
            return 0;
        const eastl::hash<entt::id_type> hasher;
        return hasher(entt::to_integral(np));
    }
};
}
