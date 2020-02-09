/*************************************************************************/
/*  vector.h                                                             */
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

/**
 * @class Vector
 * @author Juan Linietsky
 * Vector container. Regular Vector Container. Use with care and for smaller arrays when possible. Use PoolVector for large arrays.
*/

#include "core/cowdata.h"
#include "core/cowdata_impl.h"

#include "core/error_macros.h"
#include "core/os/memory.h"
#include "core/sort_array.h"
#include "EASTL/vector.h"
#include "EASTL/fixed_vector.h"
#include "EASTL/span.h"

template<class T>
using PODVector = eastl::vector<T,wrap_allocator>;
template<class T,int N,bool GROWING=false>
using FixedVector = eastl::fixed_vector<T,N,GROWING,wrap_allocator>;
template <typename T>
using Span = eastl::span<T,eastl::dynamic_extent>;
class Variant;

template <class T>
class VectorWriteProxy {
public:
    _FORCE_INLINE_ T &operator[](int p_index) {
        CRASH_BAD_INDEX(p_index, ((Vector<T> *)(this))->_cowdata.size());

        return ((Vector<T> *)(this))->_cowdata.ptrw()[p_index];
    }
};

template <class T>
class Vector {
    friend class VectorWriteProxy<T>;

public:
    VectorWriteProxy<T> write;

private:
    CowData<T> _cowdata;

public:
    bool push_back(T p_elem);
    void remove(int p_index) { _cowdata.remove(p_index); }
    void erase(const T &p_val) {
        int idx = find(p_val);
        if (idx >= 0) remove(idx);
    }
    void invert();

    T *ptrw() { return _cowdata.ptrw(); }
    const T *ptr() const { return _cowdata.ptr(); }
    void clear() { resize(0); }
    [[nodiscard]] bool empty() const noexcept { return _cowdata.empty(); }

    void set(int p_index, T p_elem) { _cowdata.set(p_index, p_elem); }
    [[nodiscard]] int size() const { return _cowdata.size(); }
    Error resize(int p_size) { return _cowdata.resize(p_size); }
    const T &operator[](int p_index) const { return _cowdata.get(p_index); }
    Error insert(int p_pos, T p_val) { return _cowdata.insert(p_pos, p_val); }
    int find(const T &p_val, int p_from = 0) const { return _cowdata.find(p_val, p_from); }

    template <class C>
    void sort_custom() {

        int len = _cowdata.size();
        if (len == 0)
            return;

        T *data = ptrw();
        SortArray<T, C> sorter;
        sorter.sort(data, len);
    }

    void sort() {

        sort_custom<_DefaultComparator<T> >();
    }

    constexpr Vector() noexcept = default;
    Vector(const Vector &p_from) { _cowdata._ref(p_from._cowdata); }
    Vector &operator=(const Vector &p_from) {
        _cowdata._ref(p_from._cowdata);
        return *this;
    }

    explicit Vector(Span<const T> p_from) {
        resize(p_from.size());
        if(!p_from.empty()) {
            eastl::copy(p_from.begin(),p_from.end(),ptrw());
        }
    }
    ~Vector() = default;
};

template <class T>
void Vector<T>::invert() {

    for (int i = 0; i < size() / 2; i++) {
        T *p = ptrw();
        SWAP(p[i], p[size() - i - 1]);
    }
}

template <class T>
bool Vector<T>::push_back(T p_elem) {

    Error err = resize(size() + 1);
    ERR_FAIL_COND_V(err, true);
    set(size() - 1, p_elem);

    return false;
}

#ifndef __MINGW32__
extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) eastl::vector<class StringName,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) eastl::vector<String,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) eastl::vector<uint8_t,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) eastl::vector<int,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) eastl::vector<float,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) eastl::vector<class Variant,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) eastl::vector<struct PropertyInfo,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) eastl::vector<struct Vector2,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) eastl::vector<struct Vector3,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) Vector<UIString>;

#endif
extern const PODVector<struct Vector2> null_vec2_pvec;
extern const PODVector<struct Vector3> null_vec3_pvec;
extern const PODVector<Variant> null_variant_pvec;
extern const PODVector<String> null_string_pvec;
extern const PODVector<int> null_int_pvec;
extern const PODVector<float> null_float_pvec;
