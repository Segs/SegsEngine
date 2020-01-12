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

template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<StringName,wrap_allocator>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::list<StringName,wrap_allocator>;

namespace
{
static Mutex *lock=nullptr;

template <typename L, typename R>
_FORCE_INLINE_ bool is_str_less(const L *l_ptr, const R *r_ptr) {

    while (true) {

        if (*l_ptr == 0 && *r_ptr == 0)
            return false;
        else if (*l_ptr == 0)
            return true;
        else if (*r_ptr == 0)
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
    _Data *prev;
    _Data *next;
    const char *cname;
    SafeRefCount refcount;
    uint32_t idx:31;
    //! if set to 1 then underlying char * array was allocated dynamically.
    uint32_t mark:1;
    uint32_t hash;

    const char *get_name() const { return cname; }
    void set_static_name(const char *s) {
        mark = 0;
        cname = s;
    }
    void set_dynamic_name(se_string_view s) {

        char *data = (char *)Memory::alloc_static(s.size()+1);
        memcpy(data,s.data(),s.size());
        data[s.size()]=0;
        cname =data;
        mark = 1;
    }
    _Data() {
        cname = nullptr;
        next = prev = nullptr;
        idx = 0;
        hash = 0;
    }
    ~_Data() {
       if(mark)  // dynamic memory
       {
           Memory::free_static((void *)get_name());
           cname = nullptr;
       }
    }
};

StringName::_Data *StringName::_table[STRING_TABLE_LEN];
bool StringName::configured = false;


void StringName::setup() {

    lock = memnew(Mutex);

    ERR_FAIL_COND(configured)
    for (int i = 0; i < STRING_TABLE_LEN; i++) {

        _table[i] = nullptr;
    }
    configured = true;
}

void StringName::cleanup() {

    { // this block is done under lock, exiting the block will release the block automatically.
        MutexLock mlocker(*lock);

        int lost_strings = 0;
        for (int i = 0; i < STRING_TABLE_LEN; i++) {

            while (_table[i]) {

                _Data *d = _table[i];
                lost_strings++;
                if (OS::get_singleton()->is_stdout_verbose()) {
                    print_line(se_string("Orphan StringName: ") + d->get_name());
                }

                _table[i] = _table[i]->next;
                memdelete(d);
            }
        }
        if (lost_strings) {
            print_verbose("StringName: " + itos(lost_strings) + " unclaimed string names at exit.");
        }
    }

    memdelete(lock);
    lock = nullptr;
    configured = false;
}

void StringName::unref() {

    ERR_FAIL_COND(!configured)
    assert(_data);
    if (_data->refcount.unref()) {
        MutexLock mlocker(*lock);

        if (_data->prev) {
            _data->prev->next = _data->next;
        } else {
            if (_table[_data->idx] != _data) {
                ERR_PRINT("BUG!")
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

bool StringName::operator!=(const StringName &p_name) const {

    // the real magic of all this mess happens here.
    // this is why path comparisons are very fast
    return _data != p_name._data;
}

StringName::operator String() const {

    if (!_data||!_data->cname)
        return String();

    return String::fromUtf8(_data->get_name());
}

String StringName::asString() const { return (String)*this; }

const char *StringName::asCString() const noexcept
{
    if (!_data||!_data->cname)
        return "";

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

StringName::StringName(const StringName &p_name) {

    _data = nullptr;

    ERR_FAIL_COND(!configured)

    if (p_name._data && p_name._data->refcount.ref()) {
        _data = p_name._data;
    }
}



StringName::StringName(const char *p_name) {

    _data = nullptr;

    ERR_FAIL_COND(!configured)

    if (!p_name || p_name[0] == 0)
        return; //empty, ignore

    MutexLock mlocker(*lock);

    uint32_t hash = StringUtils::hash(p_name);
    if(hash==2304634407)
    {
        printf("in\n");
    }
    uint32_t idx = hash & STRING_TABLE_MASK;

    _data = _table[idx];

    while (_data) {

        // compare hash first
        if (_data->hash == hash && 0==strcmp(_data->get_name(),p_name))
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
    _data->set_dynamic_name(p_name);
    _data->idx = idx;
    _data->hash = hash;
    _data->next = _table[idx];
    _data->prev = nullptr;
    if (_table[idx])
        _table[idx]->prev = _data;
    _table[idx] = _data;

}

void StringName::setupFromCString(const StaticCString &p_static_string) {

    MutexLock mlocker(*lock);

    uint32_t hash = StringUtils::hash(p_static_string.ptr);
    if(hash==2304634407)
    {
        printf("in\n");
    }

    uint32_t idx = hash & STRING_TABLE_MASK;

    _data = _table[idx];

    while (_data) {

        // compare hash first
        if (_data->hash == hash && 0==strcmp(_data->get_name(),p_static_string.ptr))
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
    _data->set_static_name(p_static_string.ptr);
    _data->idx = idx;
    _data->hash = hash;
    _data->next = _table[idx];
    _data->prev = nullptr;
    if (_table[idx])
        _table[idx]->prev = _data;
    _table[idx] = _data;

}

StringName::StringName(se_string_view p_name) {

    _data = nullptr;

    ERR_FAIL_COND(!configured)

    if (p_name.empty())
        return;

    MutexLock mlocker(*lock);

    uint32_t hash = StringUtils::hash(p_name);
    if(hash==2304634407)
    {
        printf("in\n");
    }

    uint32_t idx = hash & STRING_TABLE_MASK;

    _data = _table[idx];

    while (_data) {

        if (_data->hash == hash && p_name == se_string_view(_data->get_name()))
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

    ERR_FAIL_COND_V(!configured, StringName())

    ERR_FAIL_COND_V(!p_name, StringName())
    if (!p_name[0])
        return StringName();

    MutexLock mlocker(*lock);

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

//    ERR_FAIL_COND_V(p_name.isEmpty(), StringName())

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

StringName operator+(StringName v, se_string_view sv) {
    return StringName(se_string(v)+sv);
}
