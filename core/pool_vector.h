/*************************************************************************/
/*  pool_vector.h                                                        */
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

#include "core/os/memory.h"
#include "core/os/rw_lock.h"

#include "core/safe_refcount.h"
#include "core/error_macros.h"
#include "core/vector.h"

#include "EASTL/type_traits.h"

class Object;

struct GODOT_EXPORT MemoryPool {

    struct Alloc {

        SafeRefCount refcount;
        SafeNumeric<uint32_t> lock {0};
        void *mem = nullptr;
        size_t size = 0;
        Alloc *free_list = nullptr;
    };

    //avoid accessing these directly, must be public for template access
    static Alloc *allocs;
    static Alloc *free_list;
    static uint32_t alloc_count;
    static uint32_t allocs_used;
    static size_t total_memory;
    static size_t max_memory;

    static void setup(uint32_t p_max_allocs = (1 << 16));
    static void cleanup();
    static bool do_alloc_block(Alloc *&alloc);
    static void updateTotalMemory(int delta) {
#ifdef DEBUG_ENABLED
        doUpdate(delta);
#else
        (void)delta;
#endif
    }
    static void decTotalMemory(const Alloc *alloc) {
        updateTotalMemory(-int(alloc->size));
    }
    static void releaseBlock(Alloc *alloc);
private:
#ifdef DEBUG_ENABLED
    static void doUpdate(int delta);
#endif
};


template <class T>
class PoolVector {

    MemoryPool::Alloc *alloc;

    void _copy_on_write() {

        if (!alloc)
            return;

        //		ERR_FAIL_COND(alloc->lock>0); should not be illegal to lock this for copy on write, as it's a copy on write after all

        // Refcount should not be zero, otherwise it's a misuse of COW
        if (alloc->refcount.get() == 1)
            return; //nothing to do

        //must allocate something
        MemoryPool::Alloc *old_alloc = alloc;
        if(!MemoryPool::do_alloc_block(alloc))
            return;

        {
            Write w;
            w._ref(alloc);
            Read r;
            r._ref(old_alloc);

            int cur_elements = int(alloc->size / sizeof(T));
            T *dst = (T *)w.ptr();
            const T *src = (const T *)r.ptr();
            if constexpr (std::is_base_of<Object, T>::value) {
                for (int i = 0; i < cur_elements; i++) {
                    memnew_placement(&dst[i], T(src[i]));
                }
            } else {
                for (int i = 0; i < cur_elements; i++) {
                    memnew_placement_basic(&dst[i], T(src[i]));
                }
            }
        }
        if (old_alloc->refcount.unref()) {
            //this should never happen but..
            MemoryPool::decTotalMemory(old_alloc);
            {
                Write w;
                w._ref(old_alloc);

                if constexpr(!std::is_trivially_destructible<T>::value) {
                    int cur_elements = int(old_alloc->size / sizeof(T));
                    T *elems = (T *)w.ptr();
                    for (int i = 0; i < cur_elements; i++) {
                        elems[i].~T();
                    }
                }
            }

            MemoryPool::releaseBlock(old_alloc);
        }
    }

    void _reference(const PoolVector &p_pool_vector) {

        if (alloc == p_pool_vector.alloc)
            return;

        pv_unreference();

        if (!p_pool_vector.alloc) {
            return;
        }

        if (p_pool_vector.alloc->refcount.ref()) {
            alloc = p_pool_vector.alloc;
        }
    }

    void pv_unreference() {

        if (!alloc)
            return;

        if (!alloc->refcount.unref()) {
            alloc = nullptr;
            return;
        }

        //must be disposed!

        {
            int cur_elements = int(alloc->size / sizeof(T));

            // Don't use write() here because it could otherwise provoke COW,
            // which is not desirable here because we are destroying the last reference anyways
            Write w;
            // Reference to still prevent other threads from touching the alloc
            w._ref(alloc);
            if constexpr(!std::is_trivially_destructible<T>::value) {
                for (int i = 0; i < cur_elements; i++) {
                    w[i].~T();
                }
            }
        }

        MemoryPool::decTotalMemory(alloc);

        MemoryPool::releaseBlock(alloc);
        alloc = nullptr;
    }
    class Access {
        friend class PoolVector;

    protected:
        MemoryPool::Alloc *alloc;
        T *mem;

        _FORCE_INLINE_ void _ref(MemoryPool::Alloc *p_alloc) {
            alloc = p_alloc;
            if (alloc) {
                if (alloc->lock.increment() == 1) {
                }

                mem = (T *)alloc->mem;
            }
        }

        _FORCE_INLINE_ void _unref() {

            if (alloc) {
                if (alloc->lock.decrement() == 0) {
                }

                mem = nullptr;
                alloc = nullptr;
            }
        }

        Access() {
            alloc = nullptr;
            mem = nullptr;
        }

    public:
        virtual ~Access() {
            _unref();
        }

        void release() {
            _unref();
        }
    };

public:
    using ValueType = T;


    class Read : public Access {
    public:
        const T &operator[](uint32_t p_index) const { return this->mem[p_index]; }
        const T *ptr() const { return this->mem; }

        Read &operator=(const Read &p_read) {
            if (this->alloc == p_read.alloc)
                return *this;
            this->_unref();
            this->_ref(p_read.alloc);
            return *this;
        }

        Read(const Read &p_read) {
            this->_ref(p_read.alloc);
        }

        Read() = default;
    };

    class Write : public Access {
    public:
        T &operator[](int p_index) const { return this->mem[p_index]; }
        [[nodiscard]] T *ptr() const { return this->mem; }

        Write &operator=(const Write &p_read) {
            if (this->alloc == p_read.alloc)
                return *this;
            this->_unref();
            this->_ref(p_read.alloc);
            return *this;
        }

        Write(const Write &p_read) {
            this->_ref(p_read.alloc);
        }

        Write() = default;
    };

    [[nodiscard]] Read read() const {

        Read r;
        if (alloc) {
            r._ref(alloc);
        }
        return r;
    }
    [[nodiscard]] Write write() {

        Write w;
        if (alloc) {
            _copy_on_write(); //make sure there is only one being accessed
            w._ref(alloc);
        }
        return w;
    }

    template <class MC>
    void fill_with(const MC &p_mc) {

        int c = p_mc.size();
        resize(c);
        Write w = write();
        int idx = 0;
        for (const typename MC::Element *E = p_mc.front(); E; E = E->next()) {

            w[idx++] = E->get();
        }
    }

    void remove(int p_index) {

        int s = size();
        ERR_FAIL_INDEX(p_index, s);
        Write w = write();
        for (int i = p_index; i < s - 1; i++) {

            w[i] = w[i + 1];
        }
        w = Write();
        resize(s - 1);
    }
    [[nodiscard]] bool contains(const T &v) const {
        auto rd=read();
        for(int i=0,fin=size(); i<fin; ++i)
            if(rd[i]==v)
                return true;
        return false;
    }
    int size() const;
    bool empty() const;
    void clear() { pv_unreference(); }
    const T & get(int p_index) const;
    void set(int p_index, const T &p_val);
    void push_back(const T &p_val);
    void append(const T &p_val) { push_back(p_val); }
    void append_array(const PoolVector<T> &p_arr) {
        int ds = p_arr.size();
        if (ds == 0)
            return;
        int bs = size();
        resize(bs + ds);
        Write w = write();
        Read r = p_arr.read();
        for (int i = 0; i < ds; i++)
            w[bs + i] = r[i];
    }
    PoolVector<T> subarray(int p_from, int p_to) {

        if (p_from < 0) {
            p_from = size() + p_from;
        }
        if (p_to < 0) {
            p_to = size() + p_to;
        }

        ERR_FAIL_INDEX_V(p_from, size(), PoolVector<T>());
        ERR_FAIL_INDEX_V(p_to, size(), PoolVector<T>());

        PoolVector<T> slice;
        int span = 1 + p_to - p_from;
        slice.resize(span);
        Read r = read();
        Write w = slice.write();
        for (int i = 0; i < span; ++i) {
            w[i] = r[p_from + i];
        }

        return slice;
    }

    Error insert(int p_pos, const T &p_val) {

        int s = size();
        ERR_FAIL_INDEX_V(p_pos, s + 1, ERR_INVALID_PARAMETER);
        resize(s + 1);
        {
            Write w = write();
            for (int i = s; i > p_pos; i--)
                w[i] = w[i - 1];
            w[p_pos] = p_val;
        }

        return OK;
    }

    const T & operator[](int p_index) const;

    [[nodiscard]] eastl::span<const T> toSpan() const {
        return { read().ptr(),size_t(size())};
    }

    Error resize(int p_size);

    PoolVector & operator=(const PoolVector &p_pool_vector) { _reference(p_pool_vector); return *this; }
    PoolVector & operator=(PoolVector &&p_pool_vector) noexcept {
        pv_unreference();
        alloc=p_pool_vector.alloc;
        p_pool_vector.alloc=nullptr;
        return *this;
    }
    PoolVector(PoolVector&& from) noexcept {
        alloc = from.alloc;
        from.alloc = nullptr;
    }
    PoolVector() : alloc(nullptr) {}
    explicit PoolVector(Vector<T> &from) : PoolVector() {
        resize(from.size());
        auto wr(write());
        int idx=0;
        for(T &v : from) {
            wr[idx++] = eastl::move(v);
        }
    }
    PoolVector(const PoolVector &p_pool_vector) {
        alloc = nullptr;
        _reference(p_pool_vector);
    }
    ~PoolVector() { pv_unreference(); }
};

template <class T>
int PoolVector<T>::size() const {

    return alloc ? int(alloc->size / sizeof(T)) : 0;
}

template <class T>
bool PoolVector<T>::empty() const {

    return alloc ? alloc->size == 0 : true;
}

template <class T>
const T &PoolVector<T>::get(int p_index) const {

    return operator[](p_index);
}

template <class T>
void PoolVector<T>::set(int p_index, const T &p_val) {

    ERR_FAIL_INDEX(p_index, size());

    Write w = write();
    w[p_index] = p_val;
}

template <class T>
void PoolVector<T>::push_back(const T &p_val) {

    resize(size() + 1);
    set(size() - 1, p_val);
}

template <class T>
const T & PoolVector<T>::operator[](int p_index) const {

    CRASH_BAD_INDEX(p_index, size());

    Read r = read();
    return r[p_index];
}

template <class T>
Error PoolVector<T>::resize(int p_size) {

    {
        if (unlikely(p_size < 0)) {
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__,
                    "Condition ' \"" _STR(p_size < 0) "\" ' is true. returned: " _STR(ERR_INVALID_PARAMETER),
                    DEBUG_STR("Size of PoolVector cannot be negative."));
            return ERR_INVALID_PARAMETER;
        }
    }

    if (alloc == nullptr) {

        if (p_size == 0)
            return OK; //nothing to do here
        if(!MemoryPool::do_alloc_block(alloc))
            return ERR_OUT_OF_MEMORY;
    } else {

        ERR_FAIL_COND_V_MSG(alloc->lock.get() > 0, ERR_LOCKED, "Can't resize PoolVector if locked."); //can't resize if locked!
    }

    size_t new_size = sizeof(T) * p_size;

    if (alloc->size == new_size)
        return OK; //nothing to do

    if (p_size == 0) {
        pv_unreference();
        return OK;
    }

    _copy_on_write(); // make it unique

    MemoryPool::updateTotalMemory(int(int64_t(new_size)-int64_t(alloc->size)));

    int cur_elements = int(alloc->size / sizeof(T));

    if (p_size > cur_elements) {

        if (alloc->size == 0) {
            alloc->mem = memalloc(new_size);
        } else {
            alloc->mem = memrealloc(alloc->mem, new_size);
        }

        alloc->size = new_size;

        Write w = write();
        if constexpr(std::is_base_of<Object, T>::value) {
            for (int i = cur_elements; i < p_size; ++i) {
                memnew_placement(&w[i], T);
            }
        } else {
            for (int i = cur_elements; i < p_size; ++i) {
                memnew_placement_basic(&w[i], T);
            }
        }

    } else {

        {
            Write w = write();
            if constexpr (!std::is_trivially_destructible<T>::value) {
                for (int i = p_size; i < cur_elements; i++) {
                    w[i].~T();
                }
            }
        }

        if (new_size == 0) {
            MemoryPool::releaseBlock(alloc);
        } else {
            alloc->mem = memrealloc(alloc->mem, new_size);
            alloc->size = new_size;
        }
    }

    return OK;
}

template <class T>
void invert(PoolVector<T> &v) {
    typename PoolVector<T>::Write w = v.write();
    int s = v.size();
    int half_s = s / 2;

    for (int i = 0; i < half_s; i++) {
        T temp = w[i];
        w[i] = w[s - i - 1];
        w[s - i - 1] = temp;
    }
}

#ifndef __MINGW32__
extern template class EXPORT_TEMPLATE_DECL PoolVector<unsigned char>;
extern template class EXPORT_TEMPLATE_DECL PoolVector<struct Vector2>;
extern template class EXPORT_TEMPLATE_DECL PoolVector<struct Vector3>;
extern template class EXPORT_TEMPLATE_DECL PoolVector<String>;
#endif
