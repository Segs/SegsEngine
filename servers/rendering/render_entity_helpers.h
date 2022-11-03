#pragma once

#include "core/ecs_registry.h"
#include "core/engine_entities.h"

#include "entt/entity/entity.hpp"

//template <typename T>
//class checking_handle
//{
//    RenderingEntity self;
//    const entt::registry &registry;
//    T *initial_ptr;
//    bool are_we_valid() const {
//        return initial_ptr==(registry.valid(self) ? registry.try_get<T>(self) : nullptr);
//    }
//public:
//    checking_handle(const entt::registry &RenderingEntity e) : registry(reg), self(e) {
//        initial_ptr = reg.valid(e) ? reg.try_get<T>(e) : nullptr;
//    }
//    operator T *() const {
//        ENTT_ASSERT(are_we_valid());
//        return initial_ptr;
//    }
//    ~checking_handle() {
//        ENTT_ASSERT(are_we_valid());
//    }
//};

struct MoveOnlyEntityHandle {
    RenderingEntity value;
    operator RenderingEntity() const {
        return value;
    }

    MoveOnlyEntityHandle(const MoveOnlyEntityHandle &) = delete;
    MoveOnlyEntityHandle &operator=(const MoveOnlyEntityHandle &) = delete;

    MoveOnlyEntityHandle(MoveOnlyEntityHandle &&f) noexcept :  value(f.value) {f.value=entt::null;}
    MoveOnlyEntityHandle &operator=(MoveOnlyEntityHandle &&f) noexcept {
        value = f.value;
        f.value = entt::null;
        return *this;
    }
    MoveOnlyEntityHandle &operator=(entt::null_t f) noexcept {
        value = f;
        return *this;
    }
    bool operator==(RenderingEntity e) const { return value==e; }
    bool operator==(entt::null_t v) const { return value==v; }
    bool operator!=(entt::null_t v) const { return value!=v; }

    MoveOnlyEntityHandle(RenderingEntity v) : value(v) {}
    constexpr MoveOnlyEntityHandle(entt::null_t) noexcept : value(entt::null) {}
    constexpr MoveOnlyEntityHandle() : value(entt::null) {}
    ~MoveOnlyEntityHandle()  noexcept { value=entt::null; }
};

template<class T>
struct MoveOnlyPointer {
    T *value;
    operator T *() const {
        return value;
    }
    template<typename U>
    operator U *() const {
        return static_cast<U *>(value);
    }

    T& operator*() const noexcept { return *value; }
    constexpr T* operator->() const noexcept { return value; }


    MoveOnlyPointer(const MoveOnlyPointer &) = delete;
    MoveOnlyPointer &operator=(const MoveOnlyPointer &) = delete;

    MoveOnlyPointer(MoveOnlyPointer &&f) noexcept :  value(f.value) {f.value=nullptr;}
    MoveOnlyPointer &operator=(MoveOnlyPointer &&f) noexcept {
        value = f.value;
        f.value = nullptr;
        return *this;
    }
    MoveOnlyPointer &operator=(T *f) noexcept {
        value = f;
        return *this;
    }


    bool operator!=(T *e) const { return value!=e; }
    bool operator==(T *e) const { return value==e; }

    MoveOnlyPointer(T *v) : value(v) {}
    MoveOnlyPointer() : value(nullptr) {}
    ~MoveOnlyPointer()  noexcept {}
};
