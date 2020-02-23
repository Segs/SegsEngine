/*************************************************************************/
/*  ustring.h                                                            */
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
//#define QT_RESTRICTED_CAST_FROM_ASCII
#include "core/typedefs.h"
#include "core/hashfuncs.h"

#include <QtCore/QString>


class Array;
class Variant;
using CharString = QByteArray;
using CharProxy = QCharRef;
using CharType = QChar;
using StringView = QStringView;
using UIString = QString;
extern const UIString s_null_ui_string; // used to return 'null' string reference

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

//  Replicated from qhashfunctions to prevent including the whole thing
QT_BEGIN_NAMESPACE
Q_CORE_EXPORT Q_DECL_PURE_FUNCTION uint qHash(const QString &key, uint seed0) noexcept;
QT_END_NAMESPACE

namespace eastl {
    template<>
    struct hash<CharType> {
        size_t operator()(CharType np) const {
            return eastl::hash<uint16_t>()(np.unicode());
        }

    };
    template<>
    struct hash<UIString> {
        size_t operator()(const UIString &np) const {
            return qHash(np,0);
        }

    };
}

#include "core/string_utils.h"

