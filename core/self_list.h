/*************************************************************************/
/*  self_list.h                                                          */
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
#include "core/error_macros.h"


class InListNodeBase {
public:    
    class IntrusiveListBase* _root;
    InListNodeBase* _next;
    InListNodeBase* _prev;

    _FORCE_INLINE_ bool in_list() const { return _root; }

};

class IntrusiveListBase {
    template<class T>
    friend class IntrusiveListNode;
protected:
    InListNodeBase* _first = nullptr;
    InListNodeBase* _last = nullptr;

    void add(InListNodeBase* p_elem) {

        ERR_FAIL_COND(p_elem->_root);

        p_elem->_root = this;
        p_elem->_next = _first;
        p_elem->_prev = nullptr;

        if (_first) {
            _first->_prev = p_elem;

        }
        else {
            _last = p_elem;
        }

        _first = p_elem;
    }
    void add_last(InListNodeBase* p_elem) {

        ERR_FAIL_COND(p_elem->_root);

        p_elem->_root = this;
        p_elem->_next = nullptr;
        p_elem->_prev = _last;

        if (_last) {
            _last->_next = p_elem;

        }
        else {
            _first = p_elem;
        }

        _last = p_elem;
    }

    void remove(InListNodeBase* p_elem) {

        ERR_FAIL_COND(p_elem->_root != this);
        if (p_elem->_next) {
            p_elem->_next->_prev = p_elem->_prev;
        }

        if (p_elem->_prev) {
            p_elem->_prev->_next = p_elem->_next;
        }

        if (_first == p_elem) {
            _first = p_elem->_next;
        }

        if (_last == p_elem) {
            _last = p_elem->_prev;
        }

        p_elem->_next = nullptr;
        p_elem->_prev = nullptr;
        p_elem->_root = nullptr;
    }
    constexpr IntrusiveListBase() noexcept = default;
};

template <class T>
class IntrusiveListNode : public InListNodeBase  {

private:
    T* _self;

public:
    _FORCE_INLINE_ void remove_from_list() {
        if (_root) _root->remove(this);
    }
    _FORCE_INLINE_ IntrusiveListNode<T>* next() { return (IntrusiveListNode<T>*)_next; }
    _FORCE_INLINE_ IntrusiveListNode<T>* prev() { return (IntrusiveListNode<T>*)_prev; }
    _FORCE_INLINE_ const IntrusiveListNode* next() const { return (IntrusiveListNode<T>*)_next; }
    _FORCE_INLINE_ const IntrusiveListNode* prev() const { return (IntrusiveListNode<T> *)_prev; }
    T* self() const { return _self; }

    _FORCE_INLINE_ IntrusiveListNode(T* p_self) {

        _self = p_self;
        _next = nullptr;
        _prev = nullptr;
        _root = nullptr;
    }

    _FORCE_INLINE_ ~IntrusiveListNode() {

        if (_root)
            _root->remove(this);
    }
};


template <class T>
class IntrusiveList : public IntrusiveListBase {

    using NodeType = IntrusiveListNode<T>;
public:
    void add(NodeType* p_elem) {
        IntrusiveListBase::add(p_elem);
    }

    void add_last(NodeType* p_elem) {
        IntrusiveListBase::add_last(p_elem);
    }

    void remove(NodeType* p_elem) {
        IntrusiveListBase::remove(p_elem);
    }

    _FORCE_INLINE_ NodeType * first() { return (NodeType*)_first; }
    _FORCE_INLINE_ const NodeType * first() const { return (const NodeType*)_first; }
    IntrusiveList() noexcept = default;
    _FORCE_INLINE_ ~IntrusiveList() {
        if (unlikely(_first != nullptr)) {
            _err_print_error(FUNCTION_STR, __FILE__, __LINE__, "List was not cleared before destruction");
        }
    }
};
