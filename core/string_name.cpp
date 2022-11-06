/*************************************************************************/
/*  string_name.cpp                                                      */
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

#include "string_name.h"

#include "core/os/os.h"
#include "core/os/mutex.h"
#include "core/print_string.h"
#include "core/ustring.h"
#include "core/vector.h"
#include "core/string_utils.inl"

const Vector<StringName> g_null_stringname_vec; //!< Can be used wherever user needs to return/pass a const Vector<StringName> reference.

namespace
{
static Mutex lock;

template <typename L, typename R>
_FORCE_INLINE_ bool is_str_less(const L *l_ptr, const R *r_ptr) {

    while (true) {

        if (*l_ptr == 0)
            return *r_ptr != 0;
        if (*r_ptr == 0)
            return false;
        else if (*l_ptr < *r_ptr)
            return true;
        else if (*l_ptr > *r_ptr)
            return false;

        l_ptr++;
        r_ptr++;
    }
}
} // end of anonymous namespace

struct StringName::_Data {
    _Data *prev = nullptr;
    _Data *next = nullptr;
    const char *cname = nullptr;
    SafeRefCount refcount;
    uint32_t idx:31;
    //! if set to 1 then underlying char * array was allocated dynamically.
    uint32_t mark:1;
    uint32_t hash=0;

    const char *get_name() const { return cname; }
    void set_static_name(const char *s) {
        mark = 0;
        cname = s;
    }
    void set_dynamic_name(StringView s) {

        char *data = (char *)Memory::alloc(s.size()+1);
        memcpy(data,s.data(),s.size());
        data[s.size()]=0;
        cname =data;
        mark = 1;
    }
    _Data() {
        idx = 0;
        hash = 0;
    }
    ~_Data() {
       if(mark)  // dynamic memory
       {
           Memory::free((void *)get_name());
           cname = nullptr;
       }
    }
};

StringName::_Data *StringName::_table[STRING_TABLE_LEN];
bool StringName::configured = false;


void StringName::setup() {

    ERR_FAIL_COND(configured);
    for (auto &entry : _table) {
        entry = nullptr;
    }
    configured = true;
}

void StringName::cleanup(bool log_orphans) {

    { // this block is done under lock, exiting the block will release the block automatically.
        MutexLock mlocker(lock);

        int lost_strings = 0;
        for (auto &entry : _table) {

            while (entry) {

                _Data *d = entry;
                lost_strings++;
                if (log_orphans) {
                    print_line(String("Orphan StringName: ") + d->get_name());
                }

                entry = entry->next;
                memdelete(d);
            }
        }
        if (lost_strings) {
            print_verbose("StringName: " + itos(lost_strings) + " unclaimed string names at exit.");
        }
    }

    configured = false;
}

void StringName::unref() noexcept {

    ERR_FAIL_COND(!configured);
    assert(_data);
    if (_data->refcount.unref()) {
        MutexLock mlocker(lock);

        if (_data->prev) {
            _data->prev->next = _data->next;
        } else {
            if (_table[_data->idx] != _data) {
                ERR_PRINT("BUG!");
            }
            _table[_data->idx] = _data->next;
        }

        if (_data->next) {
            _data->next->prev = _data->prev;
        }
        memdelete(_data);
    }

    _data = nullptr;
}

StringName::operator const void*() const {
    return (_data && _data->cname) ? (void *)1 : nullptr;
}

uint32_t StringName::hash() const {

    return _data ? _data->hash : 0;
}

StringName::operator UIString() const {

    if (!_data||!_data->cname) {
        return UIString();
    }

    return UIString::fromUtf8(_data->get_name());
}

UIString StringName::asString() const { return (UIString)*this; }

const char *StringName::asCString() const noexcept
{
    if (!_data||!_data->cname) {
        return "";
    }

    return _data->get_name();
}

StringName &StringName::operator=(const StringName &p_name) {

    if (this == &p_name)
        return *this;

    if(_data)
        unref();

    if (p_name._data && p_name._data->refcount.ref()) {

        _data = p_name._data;
    }
    return *this;
}

StringName::StringName(const StringName &p_name) noexcept {

    _data = nullptr;

    ERR_FAIL_COND(!configured);

    if (p_name._data && p_name._data->refcount.ref()) {
        _data = p_name._data;
    }
}

void StringName::setupFromCString(StaticCString p_static_string) {
    setupFromCString(p_static_string.ptr,StringUtils::hash(p_static_string.ptr));
}

void StringName::setupFromCString(const char *ptr, uint32_t hash) {

    const uint32_t idx = hash & STRING_TABLE_MASK;
    MutexLock mlocker(lock);
    _data = _table[idx];

    while (_data) {

        // compare hash first
        if (_data->hash == hash && 0==strcmp(_data->get_name(),ptr))
            break;
        _data = _data->next;
    }

    if (_data) {
        if (_data->refcount.ref()) {
            // exists
            return;
        }
    }

    _data = memnew(_Data);

    _data->refcount.init();
    _data->set_static_name(ptr);
    _data->idx = idx;
    _data->hash = hash;
    _data->next = _table[idx];
    _data->prev = nullptr;
    if (_table[idx])
        _table[idx]->prev = _data;
    _table[idx] = _data;

}

StringName::StringName(StringView p_name) {

    _data = nullptr;

    ERR_FAIL_COND(!configured);

    if (p_name.empty())
        return;

    const uint32_t hash = StringUtils::hash(p_name);
    const uint32_t idx = hash & STRING_TABLE_MASK;

    MutexLock mlocker(lock);

    _data = _table[idx];

    while (_data) {

        if (_data->hash == hash && p_name == StringView(_data->get_name()))
            break;
        _data = _data->next;
    }

    if (_data) {
        if (_data->refcount.ref()) {
            // exists
            return;
        }
    }

    _data = memnew(_Data);
    _data->set_dynamic_name(p_name);
    _data->refcount.init();
    _data->hash = hash;
    _data->idx = idx;
    _data->next = _table[idx];
    _data->prev = nullptr;
    if (_table[idx])
        _table[idx]->prev = _data;
    _table[idx] = _data;

}


StringName StringName::search(const char *p_name) {

    ERR_FAIL_COND_V(!configured, StringName());

    ERR_FAIL_COND_V(!p_name, StringName());
    if (!p_name[0])
        return StringName();

    MutexLock mlocker(lock);

    uint32_t hash = StringUtils::hash(p_name);

    uint32_t idx = hash & STRING_TABLE_MASK;

    _Data *_data = _table[idx];

    while (_data) {

        // compare hash first
        if (_data->hash == hash && 0==strcmp(_data->get_name(),p_name))
            break;
        _data = _data->next;
    }

    if (_data && _data->refcount.ref()) {
        return StringName(_data);
    }

    return StringName(); //does not exist
}

//StringName StringName::search(const String &p_name) {

//    ERR_FAIL_COND_V(p_name.isEmpty(), StringName());

//    MutexLock mlocker(*lock);

//    uint32_t hash = StringUtils::hash(p_name);

//    uint32_t idx = hash & STRING_TABLE_MASK;

//    _Data *_data = _table[idx];

//    while (_data) {

//        // compare hash first
//        if (_data->hash == hash && p_name == _data->get_name())
//            break;
//        _data = _data->next;
//    }

//    if (_data && _data->refcount.ref()) {
//        return StringName(_data);
//    }

//    return StringName(); //does not exist
//}



bool StringName::AlphCompare(const StringName &l, const StringName &r) {

    const char *l_cname = l._data ? l._data->get_name() : "";
    const char *r_cname = r._data ? r._data->get_name() : "";

    return is_str_less(l_cname, r_cname);
}

StringName operator+(const StringName &v, StringView sv) {
    return StringName(String(v)+sv);
}
