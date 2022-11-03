/*************************************************************************/
/*  dictionary.cpp                                                       */
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

#include "dictionary.h"

#include "core/hash_map.h"
#include "core/hashfuncs.h"
#include "core/safe_refcount.h"
#include "core/variant.h"
#include <cassert>

struct DictionaryPrivate {

    SafeRefCount refcount;
    HashMap<StringName, Variant, Hasher<Variant>, VariantComparator> variant_map;
};

Vector<StringName> Dictionary::get_key_list() const {
    Vector<StringName> res;
    if (_p->variant_map.empty()) {
        return {};
    }
    res.reserve(_p->variant_map.size());
    for(const auto &E: _p->variant_map) {
        res.emplace_back(E.first);
    }
    return res;
}

StringName Dictionary::get_key_at_index(int p_index) const {

    if(p_index<0 || p_index > _p->variant_map.size()){
        return StringName();
    }
    auto iter = _p->variant_map.begin();
    eastl::advance(iter,p_index);
    return iter->first;
}

Variant Dictionary::get_value_at_index(int p_index) const {

    if (p_index<0 || p_index > _p->variant_map.size()) {
        return Variant();
    }
    auto iter = _p->variant_map.begin();
    eastl::advance(iter, p_index);
    return iter->second;
}

Variant &Dictionary::operator[](const StringName &p_key) {

    return _p->variant_map[p_key];
}

const Variant &Dictionary::operator[](const StringName &p_key) const {

    return _p->variant_map[p_key];
}
const Variant *Dictionary::getptr(const StringName &p_key) const {
    auto iter = _p->variant_map.find(p_key);

    if (_p->variant_map.end() == iter) {
        return nullptr;
    }
    return &iter->second;
}

Variant *Dictionary::getptr(const StringName &p_key) {

    auto E = _p->variant_map.find(p_key);

    if (_p->variant_map.end()==E) {
        return nullptr;
    }
    return &E->second;
}

Variant Dictionary::get_valid(const StringName &p_key) const {

    auto E = _p->variant_map.find(p_key);

    if (_p->variant_map.end() == E) {
        return Variant();
    }
    return E->second;
}

Variant Dictionary::get(const StringName &p_key, const Variant &p_default) const {
    const Variant *result = getptr(p_key);
    if (!result) {
        return p_default;
    }

    return *result;
}

int Dictionary::size() const {

    return _p->variant_map.size();
}
bool Dictionary::empty() const {

    return _p->variant_map.empty();
}

bool Dictionary::has(const StringName &p_key) const {

    return _p->variant_map.contains(p_key);
}

bool Dictionary::has_all(const Array &p_keys) const {
    for (int i = 0; i < p_keys.size(); i++) {
        if (!has(p_keys[i].as<StringName>())) {
            return false;
        }
    }
    return true;
}

bool Dictionary::erase(const StringName &p_key) {

    return _p->variant_map.erase(p_key);
}

bool Dictionary::deep_equal(const Dictionary &p_dictionary, int p_recursion_count) const {
    // Cheap checks
    ERR_FAIL_COND_V_MSG(p_recursion_count > MAX_RECURSION, 0, "Max recursion reached");
    if (_p == p_dictionary._p) {
        return true;
    }
    if (_p->variant_map.size() != p_dictionary._p->variant_map.size()) {
        return false;
    }

    // Heavy O(n) check
    auto this_E = _p->variant_map.begin();
    auto other_E = p_dictionary._p->variant_map.begin();
    p_recursion_count++;
    while (this_E != _p->variant_map.end() && other_E != p_dictionary._p->variant_map.end()) {
        if (this_E->first!=other_E->first || !this_E->second.deep_equal(other_E->second, p_recursion_count)) {
            return false;
        }

        ++this_E;
        ++other_E;
    }
    return true;
}

bool Dictionary::operator==(const Dictionary &p_dictionary) const {

    return _p == p_dictionary._p;
}

bool Dictionary::operator!=(const Dictionary &p_dictionary) const {

    return _p != p_dictionary._p;
}

void Dictionary::_ref(const Dictionary &p_from) const {

    //make a copy first (thread safe)
    if (!p_from._p->refcount.ref()) {
        return; // couldn't copy
    }

    //if this is the same, unreference the other one
    if (p_from._p == _p) {
        _p->refcount.unref();
        return;
    }
    if (_p) {
        _unref();
    }
    _p = p_from._p;
}

void Dictionary::clear() {

    _p->variant_map.clear();
}

void Dictionary::_unref() const {

    ERR_FAIL_COND(!_p);
    if (_p->refcount.unref()) {
        memdelete(_p);
    }
    _p = nullptr;
}
uint32_t Dictionary::hash() const {

    uint32_t h = hash_djb2_one_32(int(VariantType::DICTIONARY));

    for (const auto &E : _p->variant_map) {
        h = hash_djb2_one_32(E.first.hash(), h);
        h = hash_djb2_one_32(E.second.hash(), h);
    }

    return h;
}

Array Dictionary::keys() const {

    Array varr;
    if (_p->variant_map.empty()) {
        return varr;
    }

    varr.reserve(size());
    int i = 0;
    for (const auto& E : _p->variant_map) {
        varr.push_back(E.first);
        i++;
    }

    return varr;
}

Array Dictionary::values() const {

    Array varr;
    if (_p->variant_map.empty()) {
        return varr;
    }

    varr.reserve(size());
    int i = 0;
    for (const auto & E : _p->variant_map) {
        varr.push_back(E.second);
        i++;
    }

    return varr;
}

const StringName *Dictionary::next(const StringName *p_key) const {

    if (p_key == nullptr) {
        // caller wants to get the first element
        if (!_p->variant_map.empty()) {
            return &_p->variant_map.begin()->first;
        }
        return nullptr;
    }
    auto E = _p->variant_map.find(*p_key);
    if (E == _p->variant_map.end()) {
        return nullptr;
    }
    ++E;
    if (E == _p->variant_map.end()) {
        return nullptr;
    }
    return &E->first;
}
void *Dictionary::id() const {
    return _p;
}
Dictionary Dictionary::duplicate(bool p_deep) const {

    Dictionary n;

    for (const auto & E : _p->variant_map) {
        n[E.first] = p_deep ? E.second.duplicate(true) : E.second;
    }

    return n;
}

Dictionary::Dictionary(const Dictionary &p_from) {
    _p = nullptr;
    _ref(p_from);
}

Dictionary::Dictionary() {

    _p = memnew(DictionaryPrivate);
    _p->refcount.init();
}
Dictionary::~Dictionary() {
    if (_p) {
        _unref();
    }
}
