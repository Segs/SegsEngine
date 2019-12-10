/*************************************************************************/
/*  ustring.h                                                            */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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
//#define QT_RESTRICTED_CAST_FROM_ASCII
#include "core/typedefs.h"
#include "core/hashfuncs.h"

#include <QtCore/QString>

class Array;
class Variant;
template <class T>
class Vector;
using CharString = QByteArray;
using CharProxy = QCharRef;
using CharType = QChar;
using StringView = QStringView;
using String = QString;
extern const String s_null_ui_string; // used to return 'null' string reference

// Platform specific wchar_t string
using WString = std::wstring;

struct StrRange {

    const CharType *c_str;
    int len;

    StrRange(const CharType *p_c_str = nullptr, int p_len = 0) {
        c_str = p_c_str;
        len = p_len;
    }
};

//class GODOT_EXPORT String {

//    static const CharType _null;
//public:
//    static const String null_val;
//    QString m_str;
//    void clear() { m_str.clear(); }
//    CharType *data() {
//        return m_str.data();
//    }
//    const CharType *cdata() const {
//        return m_str.constData();
//    }
//    CharType front() const { return m_str.front(); }
//    CharType back() const { return m_str.back(); }
//    //void remove(int p_index) { this->re.remove(p_index); }

//    const CharType operator[](int p_index) const {
//        if(p_index==size())
//            return CharType(); // Godot logic assumes accessing str[length] will return null char :|
//        return m_str[p_index];
//    }
//    _FORCE_INLINE_ void set(int p_index, CharType p_elem) {
//        m_str[p_index] = p_elem;
//    }

//    /* Compatibility Operators */

//    bool operator==(const StrRange &p_str_range) const;

//    static String asprintf(const char *format, ...) Q_ATTRIBUTE_FORMAT_PRINTF(1, 2);

//    _FORCE_INLINE_ bool empty() const { return m_str.isEmpty(); }
//    [[nodiscard]] int element_count() const { return m_str.size(); }
//    /*[[deprecated]] */ int size() const { return m_str.size(); }
//    /*[[deprecated]] */ int length() const { return m_str.length(); }
//    /**
//     * The constructors must not depend on other overloads
//     */
//    bool operator!=(const char *v) const { return m_str!=v; }
//    bool operator==(const String &v) const { return m_str==v.m_str; }
//    bool operator!=(const String &v) const { return m_str!=v.m_str; }
//    bool operator<(const String &b) const{ return m_str<b.m_str;}
//    bool operator<=(const String &b) const { return m_str<=b.m_str;}
//    bool operator==(const char *v) const { return m_str==v; }

//    String &operator+=(const String &rhs) {
//        m_str += rhs.m_str;
//        return *this;
//    }
//    String &operator+=(StringView rhs) {
//        m_str.append(rhs.data(),rhs.size());
//        return *this;
//    }
//    String &operator+=(CharType rhs) {
//        m_str += rhs;
//        return *this;
//    }
//    String operator+(const String &rhs) const {
//        return m_str + rhs.m_str;
//    }
//    String operator+(const char *rhs) const {
//        return m_str + QString(rhs);
//    }
//    String operator+(const CharType rhs) const {
//        return m_str + QString(rhs);
//    }

//    String() = default;
//    String(std::nullptr_t ) : m_str() {}
//    String(const StrRange &p_range);
//    String(QString &&o) : m_str(std::move(o)) {}
//    String(const QString &o) : m_str(o) {}
//    String(const String &o) = default;
//    String(String &&o) = default;
//    explicit String(CharType x) : m_str(x) {}
//    explicit String(se_string_view x);
//    //TODO: mark const char * String constructor as explicit to catch all manner of dynamic allocations.
//    explicit String(const char *s) : m_str(s) {}
//    explicit String(const CharType *s,int size=-1) { m_str = QString(s,size); }
//    String &operator=(const String &) = default;
//    String &operator=(String &&) = default;
//    String &operator=(const char *s) { m_str = s; return *this; }
//    String(StringView v) : m_str(v.toString()) {}

//};
//inline String operator+(const char *a,const String &b) { return String(a)+b;}
//inline String operator+(CharType a,const String &b) { return String(a+b.m_str);}
//inline bool operator==(const char *a,const String &b) { return a==b.m_str;}

#include "core/string_utils.h"

