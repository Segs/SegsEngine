/*************************************************************************/
/*  string_name.h                                                        */
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
#include "core/safe_refcount.h"
#include "core/error_macros.h"
#include <cstddef>

#include "EASTL/string_view.h"

using UIString = class QString;

struct StaticCString {

    const char *ptr;
    template<std::size_t N>
    constexpr explicit StaticCString(char const (&s)[N]) : ptr(s) {}
    constexpr StaticCString(const char *v,bool /*force*/) : ptr(v) {}
    constexpr StaticCString() : ptr(nullptr) {}
    constexpr StaticCString(StaticCString &&) = default;
    constexpr StaticCString(const StaticCString &) = default;
    constexpr operator bool() const { return ptr!=nullptr;}
};

class GODOT_EXPORT StringName {

    enum {

        STRING_TABLE_BITS = 12,
        STRING_TABLE_LEN = 1 << STRING_TABLE_BITS,
        STRING_TABLE_MASK = STRING_TABLE_LEN - 1
    };

    struct _Data;

    GODOT_NO_EXPORT static _Data *_table[STRING_TABLE_LEN];
    GODOT_NO_EXPORT static void setup();
    GODOT_NO_EXPORT static void cleanup();
    static bool configured;

    _Data *_data;

    void unref() noexcept;
    friend void register_core_types();
    friend void unregister_core_types();

    void setupFromCString(const StaticCString &p_static_string);
    explicit StringName(_Data *p_data) { _data = p_data; }

public:
    operator const void *() const;

    bool operator==(se_string_view p_name) const {

        if (!_data) {
            return p_name.empty();
        }

        return p_name.compare(asCString())==0;
    }

    bool operator==(const char *p_name) const noexcept {
        return *this==se_string_view(p_name);
    }
    bool operator!=(se_string_view p_name) const {
        return !(operator==(p_name));
    }
    bool operator<(const StringName &p_name) const {
        return _data < p_name._data;
    }
    bool operator==(const StringName &p_name) const {
        // the real magic of all this mess happens here.
        // this is why path comparisons are very fast
        return _data == p_name._data;
    }
    [[nodiscard]] uint32_t hash() const;

    const void *data_unique_pointer() const {
        return (void *)_data;
    }
    bool operator!=(const StringName &p_name) const noexcept {

        // the real magic of all this mess happens here.
        // this is why path comparisons are very fast
        return _data != p_name._data;
    }

    StringName& operator=(StringName &&p_name)
    {
        if(this==&p_name)
            return *this;
        if(_data)
            unref();
        _data = p_name._data;
        p_name._data = nullptr;
        return *this;
    }

    StringName& operator=(const StringName &p_name);
    explicit operator UIString() const;
    operator se_string_view() const noexcept
    {
        return se_string_view(asCString());
    }

    [[nodiscard]] UIString asString() const;
    [[nodiscard]] const char *asCString() const noexcept;

    static StringName search(const char *p_name);

    static StringName search(const UIString &p_name);

    static bool AlphCompare(const StringName &l, const StringName &r);

    [[nodiscard]] constexpr bool empty() const { return _data == nullptr; }

    //Marked as explicit since it *will* allocate memory
    explicit StringName(const char *p_name);


    StringName(const StringName &p_name) noexcept;
    StringName(StringName &&p_name) noexcept
    {
        _data = p_name._data;
        p_name._data = nullptr;
    }
    //TODO: mark StringName(const String &p_name) explicit, it allocates some memory, even if COW'ed
    explicit StringName(se_string_view p_name);

    StringName(const StaticCString &p_static_string) {
        _data = nullptr;

        ERR_FAIL_COND(!configured);

        if (unlikely(!p_static_string.ptr || !p_static_string.ptr[0])) {
            ERR_REPORT_COND(!p_static_string.ptr || !p_static_string.ptr[0]);
            return;
        }
        setupFromCString(p_static_string);
    }
    constexpr StringName() noexcept : _data(nullptr) {}

    template<std::size_t N>
    GODOT_NO_EXPORT StringName(char const (&s)[N]) {
        _data = nullptr;

        if constexpr (N<=1) // static zero-terminated string of length 1 is just \000
            return;
        //TODO: consider compile-time hash and index generation
        ERR_FAIL_COND(!configured);
        setupFromCString(StaticCString(s));
    }

    ~StringName() noexcept {
        if(_data)
            unref();
    }
};
GODOT_EXPORT StringName operator+(StringName v,se_string_view sv);
extern const Vector<StringName> g_null_stringname_vec;

struct WrapAlphaCompare
{
    bool operator()(const StringName &a,const StringName &b) const {
        return StringName::AlphCompare(a,b);
    }
};
struct SNSVComparer {
    bool operator()(const StringName &s,se_string_view b) const {
        return se_string_view(s)<b;
    }
    bool operator()(se_string_view a,const StringName &b) const {
        return a<se_string_view(b);
    }
};
namespace eastl {
template <typename T> struct hash;
template <>
struct hash<StringName> {
    size_t operator()(const StringName &val) const { return val.hash(); }
};
}
