/*************************************************************************/
/*  list.h                                                               */
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
#include "core/sort_array.h"
#include "EASTL/list.h"

template<class T>
using List = eastl::list<T,wrap_allocator>;

extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) eastl::list<class StringName,wrap_allocator>;
extern template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) eastl::list<StringView,wrap_allocator>;

/**
 * Generic Templatized Linked List Implementation.
 * The implementation differs from the STL one because
 * a compatible preallocated linked list can be written
 * using the same API, or features such as erasing an element
 * from the iterator.
 */

template <class T>
class ListOld {

    struct _Data;

public:
    class Element {

    private:
        friend class ListOld<T>;

        T value;
        Element *next_ptr;
        Element *prev_ptr;
        _Data *data;

    public:
        /**
         * Get NEXT Element iterator, for constant lists.
         */
        [[nodiscard]] _FORCE_INLINE_ const Element *next() const {

            return next_ptr;
        }
        /**
         * Get NEXT Element iterator,
         */
        _FORCE_INLINE_ Element *next() {

            return next_ptr;
        }

        /**
         * Get PREV Element iterator, for constant lists.
         */
        [[nodiscard]] _FORCE_INLINE_ const Element *prev() const {

            return prev_ptr;
        }
        /**
         * Get PREV Element iterator,
         */
        _FORCE_INLINE_ Element *prev() {

            return prev_ptr;
        }

        /**
         * * operator, for using as *iterator, when iterators are defined on stack.
         */
        _FORCE_INLINE_ const T &operator*() const {
            return value;
        }
        /**
         * operator->, for using as iterator->, when iterators are defined on stack, for constant lists.
         */
        _FORCE_INLINE_ const T *operator->() const {

            return &value;
        }
        /**
         * * operator, for using as *iterator, when iterators are defined on stack,
         */
        _FORCE_INLINE_ T &operator*() {
            return value;
        }
        /**
         * operator->, for using as iterator->, when iterators are defined on stack, for constant lists.
         */
        _FORCE_INLINE_ T *operator->() {
            return &value;
        }

        /**
         * get the value stored in this element.
         */
        _FORCE_INLINE_ T &deref() {
            return value;
        }
        /**
         * get the value stored in this element, for constant lists
         */
        [[nodiscard]] _FORCE_INLINE_ const T &deref() const {
            return value;
        }
        _FORCE_INLINE_ Element() {
            next_ptr = nullptr;
            prev_ptr = nullptr;
            data = nullptr;
        }
    };

private:
    struct _Data {

        Element *first;
        Element *last;
        int size_cache;

        bool erase(const Element *p_I) {

            ERR_FAIL_COND_V(!p_I, false);
            ERR_FAIL_COND_V(p_I->data != this, false);

            if (first == p_I) {
                first = p_I->next_ptr;
            }

            if (last == p_I)
                last = p_I->prev_ptr;

            if (p_I->prev_ptr)
                p_I->prev_ptr->next_ptr = p_I->next_ptr;

            if (p_I->next_ptr)
                p_I->next_ptr->prev_ptr = p_I->prev_ptr;

            memdelete(const_cast<Element *>(p_I));
            size_cache--;

            return true;
        }
    };

    _Data *_data;

public:
    using iterator = Element *;

    /**
    * return a const iterator to the beginning of the list.
    */
    [[nodiscard]] _FORCE_INLINE_ const Element *front() const {

        return _data ? _data->first : nullptr;
    }

    /**
    * return an iterator to the beginning of the list.
    */
    _FORCE_INLINE_ Element *front() {
        return _data ? _data->first : nullptr;
    }

    /**
    * return a const iterator to the last member of the list.
    */
    [[nodiscard]] _FORCE_INLINE_ const Element *back() const {

        return _data ? _data->last : nullptr;
    }

    /**
    * return an iterator to the last member of the list.
    */
    _FORCE_INLINE_ Element *back() {

        return _data ? _data->last : nullptr;
    }

    /**
     * store a new element at the end of the list
     */
    Element *push_back(const T &value) {

        if (!_data) {

            _data = memnew(_Data);
            _data->first = nullptr;
            _data->last = nullptr;
            _data->size_cache = 0;
        }

        Element *n = memnew(Element);
        n->value = (T &)value;

        n->prev_ptr = _data->last;
        n->next_ptr = nullptr;
        n->data = _data;

        if (_data->last) {

            _data->last->next_ptr = n;
        }

        _data->last = n;

        if (!_data->first)
            _data->first = n;

        _data->size_cache++;

        return n;
    }

    void pop_back() {

        if (_data && _data->last)
            erase(_data->last);
    }

    void pop_front() {

        if (_data && _data->first)
            erase(_data->first);
    }

    /**
     * find an element in the list,
     */
    template <class T_v>
    Element *find(const T_v &p_val) {

        Element *it = front();
        while (it) {
            if (it->value == p_val) return it;
            it = it->next();
        }

        return nullptr;
    }

    /**
     * erase an element in the list, by iterator pointing to it. Return true if it was found/erased.
     */
    bool erase(const Element *p_I) {

        if (_data) {
            bool ret = _data->erase(p_I);

            if (_data->size_cache == 0) {
                memdelete(_data);
                _data = nullptr;
            }

            return ret;
        }

        return false;
    }

    /**
     * erase the first element in the list, that contains value
     */
    bool erase(const T &value) {

        Element *I = find(value);
        return erase(I);
    }

    /**
     * return whether the list is empty
     */
    [[nodiscard]] _FORCE_INLINE_ bool empty() const {

        return (!_data || !_data->size_cache);
    }

    /**
     * clear the list
     */
    void clear() {

        while (front()) {
            erase(front());
        }
    }

    [[nodiscard]] _FORCE_INLINE_ int size() const {

        return _data ? _data->size_cache : 0;
    }

    /**
     * copy the list
     */
    ListOld &operator=(const ListOld &p_list) {

        clear();
        const Element *it = p_list.front();
        while (it) {

            push_back(it->deref());
            it = it->next();
        }
        return *this;
    }

    ListOld &operator=(ListOld &&p_list) {
        if(this==&p_list)
            return *this;
        clear();

        _data = p_list._data;
        p_list._data = nullptr;
        return *this;
    }

    template <class C>
    struct AuxiliaryComparator {

        C compare;
        bool operator()(const Element *a, const Element *b) const {

            return compare(a->value, b->value);
        }
    };

    template <class C>
    void sort_custom() {

        //this version uses auxiliary memory for speed.
        //if you don't want to use auxiliary memory, use the in_place version

        int s = size();
        if (s < 2)
            return;

        Element **aux_buffer = memnew_arr(Element *, s);

        int idx = 0;
        for (Element *E = front(); E; E = E->next_ptr) {

            aux_buffer[idx] = E;
            idx++;
        }

        SortArray<Element *, AuxiliaryComparator<C> > sort;
        sort.sort(aux_buffer, s);

        _data->first = aux_buffer[0];
        aux_buffer[0]->prev_ptr = nullptr;
        aux_buffer[0]->next_ptr = aux_buffer[1];

        _data->last = aux_buffer[s - 1];
        aux_buffer[s - 1]->prev_ptr = aux_buffer[s - 2];
        aux_buffer[s - 1]->next_ptr = nullptr;

        for (int i = 1; i < s - 1; i++) {

            aux_buffer[i]->prev_ptr = aux_buffer[i - 1];
            aux_buffer[i]->next_ptr = aux_buffer[i + 1];
        }

        memdelete_arr(aux_buffer);
    }
    template<class FUNC>
    void sort_custom(SortArray<Element *, FUNC > sort) {

        //this version uses auxiliary memory for speed.
        //if you don't want to use auxiliary memory, use the in_place version

        int s = size();
        if (s < 2)
            return;

        Element **aux_buffer = memnew_arr(Element *, s);

        int idx = 0;
        for (Element *E = front(); E; E = E->next_ptr) {

            aux_buffer[idx] = E;
            idx++;
        }

        sort.sort(aux_buffer, s);

        _data->first = aux_buffer[0];
        aux_buffer[0]->prev_ptr = nullptr;
        aux_buffer[0]->next_ptr = aux_buffer[1];

        _data->last = aux_buffer[s - 1];
        aux_buffer[s - 1]->prev_ptr = aux_buffer[s - 2];
        aux_buffer[s - 1]->next_ptr = nullptr;

        for (int i = 1; i < s - 1; i++) {

            aux_buffer[i]->prev_ptr = aux_buffer[i - 1];
            aux_buffer[i]->next_ptr = aux_buffer[i + 1];
        }

        memdelete_arr(aux_buffer);
    }
    const void *id() const {
        return _data;
    }

    /**
     * copy constructor for the list
     */
    ListOld(const ListOld &p_list) {

        _data = nullptr;
        const Element *it = p_list.front();
        while (it) {

            push_back(it->deref());
            it = it->next();
        }
    }
    ListOld(ListOld &&p_list) noexcept {
        _data = p_list._data;
        p_list._data = nullptr;
    }
    ListOld() {
        _data = nullptr;
    }
    ~ListOld() {
        clear();
        if (_data) {

            ERR_FAIL_COND(_data->size_cache);
            memdelete(_data);
        }
    }
};

