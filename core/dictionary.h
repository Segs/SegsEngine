/*************************************************************************/
/*  dictionary.h                                                         */
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
#include "core/forward_decls.h"
#include "core/array.h"
//#include "core/os/memory.h"

class Variant;
struct DictionaryPrivate;
class StringName;

class GODOT_EXPORT Dictionary {

    mutable DictionaryPrivate *_p;

    void _ref(const Dictionary &p_from) const;
    void _unref() const;

public:
    Vector<StringName> get_key_list() const;
    StringName get_key_at_index(int p_index) const;
    Variant get_value_at_index(int p_index) const;

    Variant& operator[](const StringName &p_key);
    const Variant& operator[](const StringName& p_key) const;

    const Variant *getptr(const StringName&p_key) const;
    Variant *getptr(const StringName&p_key);

    Variant get_valid(const StringName&p_key) const;
    Variant get(const StringName&p_key, const Variant &p_default) const;

    int size() const;
    bool empty() const;
    void clear();

    bool has(const StringName &p_key) const;
    bool has_all(const Array &p_keys) const;

    bool erase(const StringName&p_key);

    bool deep_equal(const Dictionary &p_dictionary, int p_recursion_count = 0) const;
    bool operator==(const Dictionary &p_dictionary) const;
    bool operator!=(const Dictionary &p_dictionary) const;

    uint32_t hash() const;

    Dictionary &operator=(const Dictionary &p_dictionary) {
        if(this==&p_dictionary) {
            return *this;
        }
        _ref(p_dictionary);
        return *this;
    }

    Dictionary &operator=(Dictionary &&p_from) noexcept {
        if (_p) {
            _unref();
        }
        // NOTE: no need to check if this == &from,
        // since in that case _p is nullptr, the code below will just cost a 2 assignemnts, instead of conditional
        _p = p_from._p;
        p_from._p = nullptr;
        return *this;
    }

    const StringName *next(const StringName*p_key = nullptr) const;

    Array keys() const;
    Vector<StringName> raw_keys() const;
    Array values() const;
    void *id() const;
    Dictionary duplicate(bool p_deep = false) const;

    Dictionary(const Dictionary &p_from);
    Dictionary(Dictionary && p_from) noexcept {
        _p = p_from._p;
        p_from._p = nullptr;
    }
    Dictionary();
    ~Dictionary();
};
