#pragma once

#include "entt/core/fwd.hpp"
#include "entt/entity/entity.hpp"
#include "core/vector.h"
#include "core/os/memory.h"

//enum class GameEntity : entt::id_type {};
//enum class RenderingEntity : entt::id_type {};
struct RenderingEntity {
    using entity_type = std::uint32_t;
    static constexpr auto null = entt::null;

    constexpr RenderingEntity(const RenderingEntity &other) noexcept
        : entt{other.entt} {}
    explicit constexpr RenderingEntity(RenderingEntity &&other) noexcept
            : entt{other.entt} { other.entt = entt::null; }

    constexpr RenderingEntity & operator=(const RenderingEntity &other) noexcept = default;
    constexpr RenderingEntity & operator=(RenderingEntity &&other) noexcept  {
        entt = other.entt;
        other.entt = entt::null;
        return *this;
    }

//    constexpr operator entity_type() const noexcept {
//        return entt;
//    }
    constexpr static RenderingEntity c(entity_type value = null) noexcept {
        return RenderingEntity(value);
    }
    constexpr RenderingEntity(entt::null_t v) noexcept  : entt{v}{
    }
    explicit constexpr RenderingEntity(entt::tombstone_t v) noexcept : entt{ v } {
    }
    constexpr RenderingEntity() noexcept  : entt{entt::null}{
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
    friend constexpr RenderingEntity RE(entity_type value) noexcept;
    friend class entt::basic_sparse_set<RenderingEntity, wrap_allocator>;
    friend struct entt::tombstone_t;
    friend struct entt::null_t;
    //friend uint32_t std::exchange<uint32_t,RenderingEntity>(uint32_t &,RenderingEntity &&);
    friend class entt::basic_registry<RenderingEntity, wrap_allocator>;
    friend class eastl::vector<RenderingEntity, wrap_allocator>;
    constexpr RenderingEntity(entity_type value) noexcept : entt{ value } {}
#ifdef _MSC_VER
    friend RenderingEntity std::exchange<RenderingEntity,uint32_t>(RenderingEntity &,uint32_t &&);
#else
    friend RenderingEntity std::__exchange<RenderingEntity,uint32_t>(RenderingEntity &,uint32_t &&);
#endif
    entity_type entt;
};
constexpr RenderingEntity RE(RenderingEntity::entity_type value) noexcept {
    return RenderingEntity(value);
}

template<>
class entt::entt_traits<RenderingEntity> : public entt::entt_traits<RenderingEntity::entity_type> {
    using super_class = entt::entt_traits<RenderingEntity::entity_type>;
public:
    [[nodiscard]] static constexpr entity_type to_integral(const RenderingEntity value) ENTT_NOEXCEPT {
        return static_cast<entity_type>(value.v());
    }
    [[nodiscard]] static constexpr entity_type to_entity(const RenderingEntity value) ENTT_NOEXCEPT {
        return super_class::to_entity(value.v());
    }
    [[nodiscard]] static constexpr entity_type to_version(const RenderingEntity value) ENTT_NOEXCEPT {
        return super_class::to_version(value.v());
    }

};

struct GameEntity {
    using entity_type = std::uint32_t;
    static constexpr auto null = entt::null;


    constexpr GameEntity(const GameEntity &other) noexcept
        : entt{other.entt} {}

//    constexpr operator entity_type() const noexcept {
//        return entt;
//    }
    constexpr static GameEntity c(entity_type value = null) noexcept {
        return GameEntity(value);
    }
    constexpr GameEntity(entt::null_t v) noexcept  : entt{v}{
    }
    constexpr GameEntity() noexcept  : entt{entt::null}{
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
    friend constexpr GameEntity GE(entity_type value) noexcept;
    friend class entt::basic_sparse_set<GameEntity, wrap_allocator>;
    friend struct entt::tombstone_t;
    //friend uint32_t std::exchange<uint32_t,GameEntity>(uint32_t &,GameEntity &&);
    friend class entt::basic_registry<GameEntity, wrap_allocator>;
    friend class eastl::vector<GameEntity, wrap_allocator>;
    constexpr GameEntity(entity_type value) noexcept : entt{ value } {}
#ifdef _MSC_VER
    friend GameEntity std::exchange<GameEntity, uint32_t>(GameEntity&, uint32_t&&);
#else
    friend GameEntity std::__exchange<GameEntity,uint32_t>(GameEntity &,uint32_t &&);
#endif
    entity_type entt;
};
constexpr GameEntity GE(GameEntity::entity_type value) noexcept {
    return GameEntity(value);
}

template<>
class entt::entt_traits<GameEntity> : public entt::entt_traits<GameEntity::entity_type> {
    using super_class = entt::entt_traits<GameEntity::entity_type>;
public:
    [[nodiscard]] static constexpr entity_type to_integral(const GameEntity value) ENTT_NOEXCEPT {
        return static_cast<entity_type>(value.v());
    }
    [[nodiscard]] static constexpr entity_type to_entity(const GameEntity value) ENTT_NOEXCEPT {
        return super_class::to_entity(value.v());
    }
    [[nodiscard]] static constexpr entity_type to_version(const GameEntity value) ENTT_NOEXCEPT {
        return super_class::to_version(value.v());
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
