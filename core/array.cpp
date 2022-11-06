/*************************************************************************/
/*  array.cpp                                                            */
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

#include "array.h"

#include "core/hashfuncs.h"
#include "core/object.h"
#include "core/math/math_funcs.h"
#include "core/pool_vector.h"
#include "core/variant.h"
#include "core/vector.h"

#include "EASTL/sort.h"

namespace {
int _clamp_slice_index(int arr_size, int p_index) {
    int fixed_index = CLAMP(p_index, -arr_size, arr_size - 1);
    if (fixed_index < 0) {
        fixed_index = arr_size + fixed_index;
    }
    return fixed_index;
}
struct _ArrayVariantSortCustom {
    Object *obj;
    StringName func;

    bool operator()(const Variant &p_l, const Variant &p_r) const {
        const Variant *args[2] = { &p_l, &p_r };
        Callable::CallError err;
        bool res = obj->call(func, args, 2, err).as<bool>();
        if (err.error != Callable::CallError::CALL_OK) {
            res = false;
        }
        return res;
    }
};
struct _ArrayVariantSort {
    bool operator()(const Variant &p_l, const Variant &p_r) const {
        bool valid = false;
        Variant res;
        Variant::evaluate(Variant::OP_LESS, p_l, p_r, res, valid);
        if (!valid) {
            return false;
        }
        return res.as<bool>();
    }
};

} // end of anonymous namespace

class ArrayPrivate {
public:
    SafeRefCount refcount;
    Vector<Variant> array;
};

void Array::_ref(const Array &p_from) const {
    ArrayPrivate *_fp = p_from._p;

    ERR_FAIL_COND(!_fp); // should NOT happen.

    if (_fp == _p) {
        return; // whatever it is, nothing to do here move along
    }

    bool success = _fp->refcount.ref();

    ERR_FAIL_COND(!success); // should really not happen either

    _unref();

    _p = p_from._p;
}

void Array::_unref() const {
    if (!_p) {
        return;
    }

    if (_p->refcount.unref()) {
        memdelete(_p);
    }
    _p = nullptr;
}

Variant &Array::operator[](int p_idx) {
    return _p->array[p_idx];
}

const Variant &Array::operator[](int p_idx) const {
    return _p->array[p_idx];
}

int Array::size() const {
    return _p->array.size();
}
bool Array::empty() const {
    return _p->array.empty();
}
void Array::clear() {
    _p->array.clear();
}

bool Array::deep_equal(const Array &p_array, int p_recursion_count) const {
    // Cheap checks
    ERR_FAIL_COND_V_MSG(p_recursion_count > MAX_RECURSION, true, "Max recursion reached");
    if (_p == p_array._p) {
        return true;
    }
    const Vector<Variant> &a1 = _p->array;
    const Vector<Variant> &a2 = p_array._p->array;
    const int size = a1.size();
    if (size != a2.size()) {
        return false;
    }

    // Heavy O(n) check
    p_recursion_count++;
    for (int i = 0; i < size; i++) {
        if (!a1[i].deep_equal(a2[i], p_recursion_count)) {
            return false;
        }
    }

    return true;
}

bool Array::operator==(const Array &p_array) const {
    return _p == p_array._p;
}

uint32_t Array::hash() const {
    uint32_t h = hash_djb2_one_32(0);

    for (const Variant &v : _p->array) {
        h = hash_djb2_one_32(v.hash(), h);
    }
    return h;
}
Array &Array::operator=(const Array &p_array) {
    if (this == &p_array) {
        return *this;
    }
    _ref(p_array);
    return *this;
}
void Array::push_back(const Variant &p_value) {
    _p->array.push_back(p_value);
}
void Array::emplace_back(Variant &&p_value) {
    _p->array.emplace_back(eastl::move(p_value));
}
void Array::push_back(const Variant *entries, int count) {
    _p->array.insert(_p->array.end(), entries, entries + count);
}

void Array::resize(uint32_t p_new_size) {
    _p->array.resize(p_new_size);
}

void Array::reserve(uint32_t p_new_size) {
    _p->array.reserve(p_new_size);
}

void Array::insert(int p_pos, const Variant &p_value) {
    _p->array.insert_at(p_pos, p_value);
}

void Array::erase(const Variant &p_value) {
    auto iter = _p->array.find(p_value);
    if (iter != _p->array.end()) {
        _p->array.erase(iter);
    }
}

const Variant &Array::front() const {
    ERR_FAIL_COND_V_MSG(_p->array.empty(), Variant::null_variant, "Can't take value from empty array.");
    return _p->array.front();
}

const Variant &Array::back() const {
    ERR_FAIL_COND_V_MSG(_p->array.empty(), Variant::null_variant, "Can't take value from empty array.");
    return _p->array.back();
}

int Array::find(const Variant &p_value, int p_from) const {
    const auto &rd(_p->array);
    for (int i = p_from, fin = _p->array.size(); i < fin; ++i) {
        if (rd[i] == p_value) {
            return i;
        }
    }
    return -1;
}

int Array::rfind(const Variant &p_value, int p_from) const {
    if (_p->array.empty()) {
        return -1;
    }

    if (p_from < 0) {
        // Relative offset from the end
        p_from = _p->array.size() + p_from;
    }
    if (p_from < 0 || p_from >= _p->array.size()) {
        // Limit to array boundaries
        p_from = _p->array.size() - 1;
    }

    for (int i = p_from; i >= 0; i--) {
        if (_p->array[i] == p_value) {
            return i;
        }
    }

    return -1;
}

int Array::count(const Variant &p_value) const {
    return eastl::count(_p->array.begin(), _p->array.end(), p_value);
}

bool Array::contains(const Variant &p_value) const {
    return find(p_value) != -1;
}

void Array::remove(int p_pos) {
    _p->array.erase_at(p_pos);
}

void Array::set(int p_idx, const Variant &p_value) {
    operator[](p_idx) = p_value;
}

const Variant &Array::get(int p_idx) const {
    return operator[](p_idx);
}

const Vector<Variant> &Array::vals() const {
    return _p->array;
}

Array Array::duplicate(bool p_deep) const {
    Array new_arr;
    int element_count = size();
    new_arr.resize(element_count);
    for (int i = 0; i < element_count; i++) {
        new_arr[i] = p_deep ? get(i).duplicate(p_deep) : get(i);
    }

    return new_arr;
}

Array Array::slice(int p_begin, int p_end, int p_step, bool p_deep) const { // like python, but inclusive on upper bound

    Array new_arr;

    ERR_FAIL_COND_V_MSG(p_step == 0, new_arr, "Array slice step size cannot be zero.");

    if (empty()) { // Don't try to slice empty arrays.
        return new_arr;
    }
    if (p_step > 0) {
        if (p_begin >= size() || p_end < -size()) {
            return new_arr;
        }
    } else { // p_step < 0
        if (p_begin < -size() || p_end >= size()) {
            return new_arr;
        }
    }

    int cur_size = size();
    int begin = _clamp_slice_index(cur_size, p_begin);
    int end = _clamp_slice_index(cur_size, p_end);

    int new_arr_size = eastl::max(((end - begin + p_step) / p_step), 0);
    new_arr.reserve(new_arr_size);

    if (likely(p_step > 0)) {
        if(p_deep) {
        for (int idx = begin; idx <= end; idx += p_step) {
                new_arr.emplace_back(eastl::move(get(idx).duplicate(p_deep)));
            }
        } else {
            for (int idx = begin; idx <= end; idx += p_step) {
                new_arr.push_back(get(idx));
            }
        }
    } else { // p_step < 0
        for (int idx = begin; idx >= end; idx += p_step) {
            new_arr.push_back(p_deep ? get(idx).duplicate(p_deep) : get(idx));
        }
    }

    return new_arr;
}

Array &Array::sort() {
    eastl::sort(_p->array.begin(), _p->array.end(), _ArrayVariantSort());
    return *this;
}

Array &Array::invert() {
    eastl::reverse(_p->array.begin(), _p->array.end());
    return *this;
}

void Array::push_front(const Variant &p_value) {
    _p->array.push_front(p_value);
}

Variant Array::pop_back() {
    if (!_p->array.empty()) {
        const int n = _p->array.size() - 1;
        Variant ret = _p->array[n];
        _p->array.resize(n);
        return ret;
    }
    return Variant();
}

Variant Array::pop_front() {
    if (!_p->array.empty()) {
        Variant ret = _p->array[0];
        _p->array.pop_front();
        return ret;
    }
    return Variant();
}

const void *Array::id() const {
    return _p;
}

Array::Array(const Array &p_from) {
    _p = nullptr;
    _ref(p_from);
}

Array::Array() {
    _p = memnew(ArrayPrivate);
    _p->refcount.init();
}
Array::Array(Vector<Variant> &&from) noexcept {
    // this function is marked noexcept even though it allocates memory, but if that allocation fails we have larger
    // problems.
    _p = memnew(ArrayPrivate);
    _p->array = eastl::move(from);
    _p->refcount.init();
}
Array::~Array() {
    _unref();
}
