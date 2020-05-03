#pragma once
#include "core/cowdata.h"
#include "core/error_macros.h"
//#include <type_traits>

class Object;

template <class T>
bool CowData<T>::_get_alloc_size_checked(size_t p_elements, size_t *out) const {
#if defined(_add_overflow) && defined(_mul_overflow)
    size_t o;
    size_t p;
    if (_mul_overflow(p_elements, sizeof(T), &o)) {
        *out = 0;
        return false;
    }
    *out = next_power_of_2(o);
    if (_add_overflow(o, static_cast<size_t>(32), &p)) return false; // no longer allocated here
    return true;
#else
    // Speed is more important than correctness here, do the operations unchecked
    // and hope the best
    *out = _get_alloc_size(p_elements);
    return true;
#endif
}

template <class T> T *CowData<T>::_get_data() const {

    if (!_ptr) return nullptr;
    return reinterpret_cast<T *>(_ptr);
}
template <class T> uint32_t *CowData<T>::_get_size() const {

    if (!_ptr) return nullptr;

    return reinterpret_cast<uint32_t *>(_ptr) - 1;
}
template <class T> uint32_t *CowData<T>::_get_refcount() const {

    if (!_ptr) return nullptr;

    return reinterpret_cast<uint32_t *>(_ptr) - 2;
}

template <class T> void CowData<T>::_unref(void *p_data) {

    if (!p_data) return;

    uint32_t *refc = _get_refcount();

    if (atomic_decrement(refc) > 0) return; // still in use
    // clean up
    if constexpr(!eastl::is_trivially_destructible<T>::value) {
            uint32_t *count = _get_size();
            T *data = (T *)(count + 1);

            for (uint32_t i = 0; i < *count; ++i) {
                // call destructors
                data[i].~T();
            }
        }

    // free mem
    Memory::free_static((uint8_t *)p_data, true);
}

template <class T> void CowData<T>::_copy_on_write() {

    if (!_ptr) return;

    uint32_t *refc = _get_refcount();

    if (unlikely(*refc > 1)) {
        /* in use by more than me */
        uint32_t current_size = *_get_size();

        uint32_t *mem_new = (uint32_t *)Memory::alloc_static(_get_alloc_size(current_size), true);

        *(mem_new - 2) = 1; // refcount
        *(mem_new - 1) = current_size; // size

        T *_data = (T *)(mem_new);

        // initialize new elements
        if constexpr(eastl::is_trivially_copyable<T>::value) { memcpy(mem_new, _ptr, current_size * sizeof(T)); }
        else {
            for (uint32_t i = 0; i < current_size; i++) {
                if constexpr(eastl::is_base_of<Object, T>::value)
                    memnew_placement(&_data[i], T(_get_data()[i]));
                else
                    memnew_placement_basic(&_data[i], T(_get_data()[i]));
            }
        }

        _unref(_ptr);
        _ptr = _data;
    }
}

template <class T> Error CowData<T>::resize(int p_size) {

    ERR_FAIL_COND_V(p_size < 0, ERR_INVALID_PARAMETER);

    if (p_size == size()) return OK;

    if (p_size == 0) {
        // wants to clean up
        _unref(_ptr);
        _ptr = nullptr;
        return OK;
    }

    // possibly changing size, copy on write
    _copy_on_write();

    size_t alloc_size;
    ERR_FAIL_COND_V(!_get_alloc_size_checked(p_size, &alloc_size), ERR_OUT_OF_MEMORY);

    if (p_size > size()) {

        if (size() == 0) {
            // alloc from scratch
            uint32_t *ptr = (uint32_t *)Memory::alloc_static(alloc_size, true);
            ERR_FAIL_COND_V(!ptr, ERR_OUT_OF_MEMORY);
            *(ptr - 1) = 0; // size, currently none
            *(ptr - 2) = 1; // refcount

            _ptr = (T *)ptr;

        } else {
            void *_ptrnew = (T *)Memory::realloc_static(_ptr, alloc_size, true);
            ERR_FAIL_COND_V(!_ptrnew, ERR_OUT_OF_MEMORY);
            _ptr = (T *)(_ptrnew);
        }

        // construct the newly created elements

        if constexpr(!eastl::is_trivially_constructible<T>::value) {
                T *elems = _get_data();

                for (int i = *_get_size(); i < p_size; i++) {
                    if constexpr(eastl::is_base_of<Object, T>::value)
                        memnew_placement(&elems[i], T);
                    else
                        memnew_placement_basic(&elems[i], T);
                }
            }

        *_get_size() = p_size;

    } else if (p_size < size()) {

        if
            constexpr(!eastl::is_trivially_destructible<T>::value) {
                // deinitialize no longer needed elements
                for (uint32_t i = p_size; i < *_get_size(); i++) {
                    T *t = &_get_data()[i];
                    t->~T();
                }
            }

        void *_ptrnew = (T *)Memory::realloc_static(_ptr, alloc_size, true);
        ERR_FAIL_COND_V(!_ptrnew, ERR_OUT_OF_MEMORY);

        _ptr = (T *)(_ptrnew);

        *_get_size() = p_size;
    }

    return OK;
}

template <class T> int CowData<T>::find(const T &p_val, int p_from) const {
    int ret = -1;

    if (p_from < 0 || size() == 0) {
        return ret;
    }

    for (int i = p_from; i < size(); i++) {
        if (get(i) == p_val) {
            ret = i;
            break;
        }
    }

    return ret;
}

template <class T> void CowData<T>::_ref(const CowData *p_from) {
    _ref(*p_from);
}

template <class T> void CowData<T>::_ref(const CowData &p_from) {

    if (_ptr == p_from._ptr) return; // self assign, do nothing.

    _unref(_ptr);
    _ptr = nullptr;

    if (!p_from._ptr) return; // nothing to do

    if (atomic_conditional_increment(p_from._get_refcount()) > 0) { // could reference
        _ptr = p_from._ptr;
    }
}

template <class T> CowData<T>::~CowData() {

    _unref(_ptr);
}

template <class T>
Error CowData<T>::insert(int p_pos, const T &p_val) {

    ERR_FAIL_INDEX_V(p_pos, size() + 1, ERR_INVALID_PARAMETER);
    resize(size() + 1);
    for (int i = (size() - 1); i > p_pos; i--)
        set(i, get(i - 1));
    set(p_pos, p_val);

    return OK;
}

template <class T>
void CowData<T>::remove(int p_index) {

    ERR_FAIL_INDEX(p_index, size());
    T *p = ptrw();
    int len = size();
    for (int i = p_index; i < len - 1; i++) {

        p[i] = p[i + 1];
    }

    resize(len - 1);
}

template <class T>
const T &CowData<T>::get(int p_index) const {

    CRASH_BAD_INDEX(p_index, size());

    return _get_data()[p_index];
}

template <class T>
T &CowData<T>::get_m(int p_index) {

    CRASH_BAD_INDEX(p_index, size());
    _copy_on_write();
    return _get_data()[p_index];
}

template <class T>
int CowData<T>::size() const {
    uint32_t *size = _get_size();
    if (size) // TODO: mark as unlikely ?
        return *size;

    return 0;
}
