/*************************************************************************/
/*  rid.h                                                                */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#pragma once

#include "core/typedefs.h"
#include "core/safe_refcount.h"
#include "core/hash_set.h"
#include "core/error_macros.h"
#include "core/engine_entities.h"
#include "entt/entity/entity.hpp"

class RID_OwnerBase;
#define RID_PRIME(a) a

class GODOT_EXPORT RID_Data {

    friend class RID_OwnerBase;

#ifndef DEBUG_ENABLED
    RID_OwnerBase *_owner;
#endif
    uint32_t _id;

public:
    _FORCE_INLINE_ uint32_t get_id() const { return _id; }

    RID_Data() {}
    virtual ~RID_Data() = default;
    RID_Data(RID_Data &&) = default;
    RID_Data & operator=(RID_Data &&) = default;
    // non-copyable
    RID_Data(const RID_Data &) = delete;
    RID_Data &operator=(const RID_Data &) = delete;
};

class GODOT_EXPORT RID {
    friend class RID_OwnerBase;

    mutable RID_Data *_data = nullptr;

public:
    //RenderingEntity eid { entt::null };

    RID_Data *get_data() const { return _data; }

    constexpr bool operator==(RID p_rid) const {
        return _data == p_rid._data;
    }
    bool operator!=(RID p_rid) const {
        return _data != p_rid._data;
    }
    bool operator<(RID p_rid) const {
        return _data < p_rid._data;
    }
    bool is_valid() const { return _data != nullptr; }
    uint32_t get_id() const { return _data ? _data->get_id() : 0; }
};


namespace eastl {
template<>
struct hash<RID> {
    size_t operator()(const RID &np) const {
        eastl::hash<size_t> hs;
        return hs(intptr_t(np.get_data())/next_power_of_2(sizeof(RID_Data)));
    }

};
}

class GODOT_EXPORT RID_OwnerBase {
public:
#ifdef DEBUG_ENABLED
    mutable HashSet<RID_Data *> id_map;
#endif
protected:
    static SafeRefCount refcount;
    _FORCE_INLINE_ void _set_data(RID &p_rid, RID_Data *p_data) {
        p_rid._data = p_data;
        refcount.ref();
        p_data->_id = refcount.get();
#ifndef DEBUG_ENABLED
        p_data->_owner = this;
#endif
    }

#ifndef DEBUG_ENABLED

    _FORCE_INLINE_ bool _is_owner(const RID &p_rid) const {

        return this == p_rid._data->_owner;
    }

    _FORCE_INLINE_ void _remove_owner(RID &p_rid) {

        p_rid._data->_owner = nullptr;
    }
#endif

public:
    void get_owned_list(Vector<RID> *p_owned);
    static void init_rid();
    virtual ~RID_OwnerBase() = default;

    void free(RID p_rid) {

#ifdef DEBUG_ENABLED
        id_map.erase(p_rid.get_data());
#else
        _remove_owner(p_rid);
#endif
    }
    _FORCE_INLINE_ bool owns(const RID &p_rid) const {

        if (p_rid.get_data() == nullptr)
            return false;
#ifdef DEBUG_ENABLED
        return id_map.contains(p_rid.get_data());
#else
        return _is_owner(p_rid);
#endif
    }
};

template <class T>
class RID_Owner : public RID_OwnerBase {

public:
    _FORCE_INLINE_ RID make_rid(T *p_data) {

        RID rid;
        _set_data(rid, p_data);

#ifdef DEBUG_ENABLED
        id_map.insert(p_data);
#endif
        return rid;
    }

    T *get(const RID &p_rid) {

#ifdef DEBUG_ENABLED

        ERR_FAIL_COND_V(!p_rid.is_valid(), nullptr);
        ERR_FAIL_COND_V(!id_map.contains(p_rid.get_data()), nullptr);
#endif
        return static_cast<T *>(p_rid.get_data());
    }

    _FORCE_INLINE_ T *getornull(const RID &p_rid) {

#ifdef DEBUG_ENABLED

        if (p_rid.get_data()) {
            ERR_FAIL_COND_V(!id_map.contains(p_rid.get_data()), nullptr);
        }
#endif
        return static_cast<T *>(p_rid.get_data());
    }

    T *getptr(const RID &p_rid) {

        return static_cast<T *>(p_rid.get_data());
    }

};
