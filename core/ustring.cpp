/*************************************************************************/
/*  ustring.cpp                                                          */
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

#include "ustring.h"

#include "se_string.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"
#include "core/color.h"
#include "core/crypto/crypto_core.h"
#include "core/math/math_funcs.h"
#include "core/os/memory.h"
#include "core/translation.h"
#include "core/translation_helpers.h"
#include "core/list.h"
#include "core/vector.h"
#include "core/variant.h"

#include <QString>
#include <QVector>
#include <QCollator>
#include <cstdio>
#include <cstdlib>
#include <QFileInfo>
#include <QTextBoundaryFinder>
#ifndef __MINGW32__
#include <charconv>

#endif
using namespace eastl;
const UIString s_null_ui_string;

/*
    TODO: SEGS: When replacing QString as the underlying string type consider the following helper class from qt
    QTextBoundaryFinder for grapheme navigation in QChar-like *strings
    QUtf8 from QtCore/private/qutfcodec_p.h>
*/

#if defined(MINGW_ENABLED) || defined(_MSC_VER)
#define snprintf _snprintf_s
#endif

int grapheme_count(se_string_view str) {
    return 0;
}
int bytes_in_next_grapheme(se_string_view str,int offset=0) {
    return 0;
}


#define MAX_DIGITS 6
namespace
{
int find_char(const UIString &s, CharType p_char) {
    return s.indexOf(p_char);
}
size_t find_char(se_string_view s, char p_char) {
    return s.find(p_char);
}
bool is_enclosed_in(const UIString &str,const CharType p_char) {

    return StringUtils::begins_with(str,p_char) && StringUtils::ends_with(str,p_char);
}
bool is_enclosed_in(se_string_view str,char p_char) {

    return str.starts_with(p_char) && str.ends_with(p_char);
}
}

bool is_symbol(CharType c) {
    return c != '_' && ((c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') || (c >= '{' && c <= '~') || c == '\t' || c == ' ');
}

bool select_word(const UIString &p_s, int p_col, int &r_beg, int &r_end) {

    const UIString &s = p_s;
    int beg = CLAMP(p_col, 0, s.length());
    int end = beg;

    if (beg == s.length() || s[beg] > 32) {

        bool symbol = beg < s.length() && is_symbol(s[beg]);

        while (beg > 0 && s[beg - 1] > 32 && (symbol == is_symbol(s[beg - 1]))) {
            beg--;
        }
        while (end < s.length() && s[end + 1] > 32 && (symbol == is_symbol(s[end + 1]))) {
            end++;
        }

        if (end < s.length())
            end += 1;

        r_beg = beg;
        r_end = end;

        return true;
    } else {

        return false;
    }
}

/** STRING **/

// assumes the following have already been validated:
// p_char != NULL
// p_length > 0
// p_length <= p_char strlen


void StringUtils::erase(UIString &str,int p_pos, int p_chars) {

    str = left(str,p_pos) + substr(str,p_pos + p_chars);
}
void StringUtils::erase(String &str,int p_pos, int p_chars) {
    str.erase(p_pos,p_chars);
}
UIString StringUtils::capitalize(const UIString &s) {

    UIString aux = strip_edges(camelcase_to_underscore(s,true).replace("_", " "));
    UIString cap;
    auto parts = split(aux,' ');
    for (int i = 0,fin=parts.size(); i < fin; ++i) {

        UIString slice(parts[i]);
        if (!slice.isEmpty()) {

            slice[0]=slice[0].toUpper();
            if (i > 0)
                cap += ' ';
            cap += slice;
        }
    }

    return cap;
}
String StringUtils::capitalize(se_string_view s) {

    String aux(strip_edges(camelcase_to_underscore(s,true).replaced("_", " ")));
    String cap;
    for (int i = 0; i < get_slice_count(aux,' '); i++) {

        se_string_view slice = get_slice(aux,' ', i);
        if (!slice.empty()) {

            //slice.set(0,StringUtils::char_uppercase(slice[0]));
            if (i > 0)
                cap += ' ';
            cap.push_back(CharToUpper(slice[0]));
            cap.append(slice.substr(1));
        }
    }

    return cap;
}
UIString StringUtils::camelcase_to_underscore(const UIString &s,bool lowercase) {
    const CharType *cstr = s.constData();
    UIString new_string;
    const char A = 'A', Z = 'Z';
    const char a = 'a', z = 'z';
    int start_index = 0;

    for (int i = 1; i < s.size(); i++) {
        bool is_upper = cstr[i] >= A && cstr[i] <= Z;
        bool is_number = cstr[i] >= '0' && cstr[i] <= '9';
        bool are_next_2_lower = false;
        bool is_next_lower = false;
        bool is_next_number = false;
        bool was_precedent_upper = cstr[i - 1] >= A && cstr[i - 1] <= Z;
        bool was_precedent_number = cstr[i - 1] >= '0' && cstr[i - 1] <= '9';

        if (i + 2 < s.size()) {
            are_next_2_lower = cstr[i + 1] >= a && cstr[i + 1] <= z && cstr[i + 2] >= a && cstr[i + 2] <= z;
        }

        if (i + 1 < s.size()) {
            is_next_lower = cstr[i + 1] >= a && cstr[i + 1] <= z;
            is_next_number = cstr[i + 1] >= '0' && cstr[i + 1] <= '9';
        }

        const bool cond_a = is_upper && !was_precedent_upper && !was_precedent_number;
        const bool cond_b = was_precedent_upper && is_upper && are_next_2_lower;
        const bool cond_c = is_number && !was_precedent_number;
        const bool can_break_number_letter = is_number && !was_precedent_number && is_next_lower;
        const bool can_break_letter_number = !is_number && was_precedent_number && (is_next_lower || is_next_number);

        bool should_split = cond_a || cond_b || cond_c || can_break_number_letter || can_break_letter_number;
        if (should_split) {
            new_string += substr(s,start_index, i - start_index) + "_";
            start_index = i;
        }
    }

    new_string += substr(s,start_index);
    return lowercase ? new_string.toLower() : new_string;
}
String StringUtils::camelcase_to_underscore(se_string_view s,bool lowercase) {
    const char *cstr = s.data();
    String new_string;
    const char A = 'A', Z = 'Z';
    const char a = 'a', z = 'z';
    int start_index = 0;

    for (size_t i = 1; i < s.size(); i++) {
        bool is_upper = cstr[i] >= A && cstr[i] <= Z;
        bool is_number = cstr[i] >= '0' && cstr[i] <= '9';
        bool are_next_2_lower = false;
        bool is_next_lower = false;
        bool is_next_number = false;
        bool was_precedent_upper = cstr[i - 1] >= A && cstr[i - 1] <= Z;
        bool was_precedent_number = cstr[i - 1] >= '0' && cstr[i - 1] <= '9';

        if (i + 2 < s.size()) {
            are_next_2_lower = cstr[i + 1] >= a && cstr[i + 1] <= z && cstr[i + 2] >= a && cstr[i + 2] <= z;
        }

        if (i + 1 < s.size()) {
            is_next_lower = cstr[i + 1] >= a && cstr[i + 1] <= z;
            is_next_number = cstr[i + 1] >= '0' && cstr[i + 1] <= '9';
        }

        const bool cond_a = is_upper && !was_precedent_upper && !was_precedent_number;
        const bool cond_b = was_precedent_upper && is_upper && are_next_2_lower;
        const bool cond_c = is_number && !was_precedent_number;
        const bool can_break_number_letter = is_number && !was_precedent_number && is_next_lower;
        const bool can_break_letter_number = !is_number && was_precedent_number && (is_next_lower || is_next_number);

        bool should_split = cond_a || cond_b || cond_c || can_break_number_letter || can_break_letter_number;
        if (should_split) {
            new_string.append(substr(s,start_index, i - start_index));
            new_string.append("_");
            start_index = i;
        }
    }

    new_string += substr(s,start_index);
    return lowercase ? to_lower(new_string) : new_string;
}
int StringUtils::get_slice_count(const UIString &str,const UIString &p_splitter) {

    auto parts = str.splitRef(p_splitter);
    return parts.size();
}
int StringUtils::get_slice_count(const UIString &str,CharType p_splitter) {

    auto parts = str.splitRef(p_splitter);
    return parts.size();
}
int StringUtils::get_slice_count(se_string_view str,char p_splitter) {
    if(str.empty())
        return 0;
    int count=1;
    auto loc = str.find(p_splitter);
    while(loc!=str.npos)
    {
        count++;
        loc = str.find(p_splitter,loc+1);
    }
    return count;
}
int StringUtils::get_slice_count(se_string_view str,se_string_view p_splitter) {
    if(str.empty() || p_splitter.empty())
        return 0;
    int count=1;
    auto loc = str.find(p_splitter);
    while(loc!=str.npos)
    {
        count++;
        loc = str.find(p_splitter,loc+1);
    }
    return count;
}
//String StringUtils::get_slice(const String &str,const String &p_splitter, int p_slice) {

//    if (p_slice<0 || str.isEmpty() || p_splitter.isEmpty())
//        return String();

//    int pos = 0;
//    int prev_pos = 0;

//    if (StringUtils::find(str,p_splitter) == -1)
//        return str;

//    int i = 0;
//    while (true) {

//        pos = StringUtils::find(str,p_splitter, pos);
//        if (pos == -1)
//            pos = str.length(); //reached end

//        int from = prev_pos;
//        //int to=pos;

//        if (p_slice == i) {

//            return StringUtils::substr(str,from, pos - from);
//        }

//        if (pos == str.length()) //reached end and no find
//            break;
//        pos += p_splitter.length();
//        prev_pos = pos;
//        i++;
//    }

//    return String(); //no find!
//}
//String StringUtils::get_slice(const String &str,se_string_view p_splitter, int p_slice) {
//    return get_slice(str,String::fromUtf8(p_splitter.data(),p_splitter.size()),p_slice);
//}
se_string_view StringUtils::get_slice(se_string_view str,se_string_view p_splitter, int p_slice) {

    if (p_slice<0 || str.empty() || p_splitter.empty())
        return se_string_view();

    size_t pos = 0;
    size_t prev_pos = 0;

    if (not contains(str,p_splitter))
        return str;

    int i = 0;
    while (true) {

        pos = StringUtils::find(str,p_splitter, pos);
        if (pos == se_string_view::npos)
            pos = str.length(); //reached end

        int from = prev_pos;
        //int to=pos;

        if (p_slice == i) {

            return substr(str,from, pos - from);
        }

        if (pos == str.length()) //reached end and no find
            break;
        pos += p_splitter.length();
        prev_pos = pos;
        i++;
    }

    return se_string_view(); //no find!
}
//String StringUtils::get_slice(const String &str,CharType p_splitter, int p_slice) {

//    if (str.isEmpty())
//        return String();

//    if (p_slice < 0)
//        return String();

//    const CharType *c = str.constData();
//    int i = 0;
//    int fin=str.size();
//    int prev = 0;
//    int count = 0;
//    for(i=0; i<fin; ++i) {

//        if (c[i] == p_splitter) {
//            if (p_slice == count) {
//                return StringUtils::substr(str,prev, i - prev);
//            } else {
//                count++;
//                prev = i + 1;
//            }
//        }
//    }
//    if (p_slice == count)
//       return StringUtils::substr(str,prev);
//    return String();
//}
se_string_view StringUtils::get_slice(se_string_view str,char p_splitter, int p_slice) {

    if (str.empty())
        return se_string_view();

    if (p_slice < 0)
        return se_string_view();

    const char *c = str.data();
    int i = 0;
    int fin=str.size();
    int prev = 0;
    int count = 0;
    for(i=0; i<fin; ++i) {

        if (c[i] == p_splitter) {
            if (p_slice == count) {
                return substr(str,prev, i - prev);
            } else {
                count++;
                prev = i + 1;
            }
        }
    }
    if (p_slice == count)
       return substr(str,prev);
    return se_string_view();
}
PODVector<UIString> StringUtils::split_spaces(const UIString &str) {

    PODVector<UIString> ret;
    int from = 0;
    int i = 0;
    int len = str.length();
    if (len == 0)
        return ret;

    bool inside = false;

    for(i=0; i<len; ++i) {

        bool empty = str[i] < 33;

        if (i == 0)
            inside = !empty;

        if (!empty && !inside) {
            inside = true;
            from = i;
        }

        if (empty && inside) {

            ret.push_back(substr(str,from, i - from));
            inside = false;
        }
    }
    ret.push_back(substr(str,from));

    return ret;
}
PODVector<se_string_view> StringUtils::split_spaces(se_string_view str) {

    PODVector<se_string_view> ret;
    int from = 0;
    int i = 0;
    int len = str.length();
    if (len == 0)
        return ret;

    bool inside = false;

    for(i=0; i<len; ++i) {

        bool empty = str[i] < 33;

        if (i == 0)
            inside = !empty;

        if (!empty && !inside) {
            inside = true;
            from = i;
        }

        if (empty && inside) {

            ret.push_back(str.substr(from, i - from));
            inside = false;
        }
    }
    ret.push_back(str.substr(from));

    return ret;
}
PODVector<UIString> StringUtils::split(const UIString &str,const UIString &p_splitter, bool p_allow_empty, int p_maxsplit) {

    PODVector<UIString> ret;
    int from = 0;
    int len = str.length();

    while (true) {

        int end = find(str,p_splitter, from);
        if (end < 0)
            end = len;
        if (p_allow_empty || (end > from)) {
            if (p_maxsplit <= 0)
                ret.push_back(substr(str,from, end - from));
            else {

                // Put rest of the string and leave cycle.
                if (p_maxsplit == ret.size()) {
                    ret.push_back(substr(str,from));
                    break;
                }

                // Otherwise, push items until positive limit is reached.
                ret.push_back(substr(str,from, end - from));
            }
        }

        if (end == len)
            break;

        from = end + p_splitter.length();
    }

    return ret;
}
PODVector<UIString> StringUtils::split(const UIString &str,const CharType p_splitter, bool p_allow_empty) {
    PODVector<UIString> ret;
    auto val = str.splitRef(p_splitter,p_allow_empty ? QString::KeepEmptyParts : QString::SkipEmptyParts);
    ret.resize(val.size());
    for(int i=0,fin=val.size(); i<fin; ++i)
        ret[i] = val[i].toString();
    return ret;
}
PODVector<se_string_view> StringUtils::split(se_string_view str,char p_splitter, bool p_allow_empty) {
    PODVector<se_string_view> ret;
    String::split_ref(ret,str,p_splitter,p_allow_empty);
    return ret;
}
PODVector<se_string_view> StringUtils::split(se_string_view str,se_string_view p_splitter, bool p_allow_empty,int p_maxsplit) {
    PODVector<se_string_view> ret;
    size_t from = 0;
    size_t len = str.length();

    while (true) {

        size_t end = StringUtils::find(str,p_splitter, from);
        if (end == String::npos)
            end = len;
        if (p_allow_empty || (end > from)) {
            if (p_maxsplit <= 0)
                ret.push_back(substr(str,from, end - from));
            else {

                // Put rest of the string and leave cycle.
                if (p_maxsplit == ret.size()) {
                    ret.push_back(substr(str,from));
                    break;
                }

                // Otherwise, push items until positive limit is reached.
                ret.push_back(substr(str,from, end - from));
            }
        }

        if (end == len)
            break;

        from = end + p_splitter.length();
    }

    return ret;
}
PODVector<UIString> StringUtils::rsplit(const UIString &str,const UIString &p_splitter, bool p_allow_empty, int p_maxsplit) {

    PODVector<UIString> ret;
    const int len = str.length();
    int remaining_len = len;

    while (true) {

        if (remaining_len < p_splitter.length() || (p_maxsplit > 0 && p_maxsplit == ret.size())) {
            // no room for another splitter or hit max splits, push what's left and we're done
            if (p_allow_empty || remaining_len > 0) {
                ret.push_back(substr(str,0, remaining_len));
            }
            break;
        }

        int left_edge = rfind(str,p_splitter, remaining_len - p_splitter.length());

        if (left_edge < 0) {
            // no more splitters, we're done
            ret.push_back(substr(str,0, remaining_len));
            break;
        }

        int substr_start = left_edge + p_splitter.length();
        if (p_allow_empty || substr_start < remaining_len) {
            ret.push_back(substr(str,substr_start, remaining_len - substr_start));
        }

        remaining_len = left_edge;
    }
    reverse(ret.begin(),ret.end());
    return ret;
}
PODVector<se_string_view> StringUtils::rsplit(se_string_view str,se_string_view p_splitter, bool p_allow_empty, int p_maxsplit) {

    PODVector<se_string_view> ret;
    const size_t len = str.length();
    const size_t split_len = p_splitter.size();
    size_t remaining_len = len;

    while (true) {

        if (remaining_len < split_len || (p_maxsplit > 0 && p_maxsplit == ret.size())) {
            // no room for another splitter or hit max splits, push what's left and we're done
            if (p_allow_empty || remaining_len > 0) {
                ret.push_back(substr(str,0, remaining_len));
            }
            break;
        }

        auto left_edge = rfind(str,p_splitter, remaining_len - split_len);

        if (left_edge == String::npos) {
            // no more splitters, we're done
            ret.push_back(substr(str,0, remaining_len));
            break;
        }

        size_t substr_start = left_edge + split_len;
        if (p_allow_empty || substr_start < remaining_len) {
            ret.push_back(substr(str,substr_start, remaining_len - substr_start));
        }

        remaining_len = left_edge;
    }

    reverse(ret.begin(),ret.end());

    return ret;
}
//Vector<float> StringUtils::split_floats(const String &str,const String &p_splitter, bool p_allow_empty) {

//    Vector<float> ret;
//    int from = 0;
//    int len = str.length();

//    while (true) {

//        int end = StringUtils::find(str,p_splitter, from);
//        if (end < 0)
//            end = len;
//        if (p_allow_empty || (end > from))
//            ret.push_back(StringUtils::to_double(str.constData()+from));

//        if (end == len)
//            break;

//        from = end + p_splitter.length();
//    }

//    return ret;
//}
PODVector<float> StringUtils::split_floats(se_string_view str,se_string_view p_splitter, bool p_allow_empty) {

    PODVector<float> ret;
    int from = 0;
    int len = str.length();

    while (true) {

        int end = StringUtils::find(str,p_splitter, from);
        if (end < 0)
            end = len;
        if (p_allow_empty || (end > from))
            ret.push_back(to_double(str.data()+from));

        if (end == len)
            break;

        from = end + p_splitter.length();
    }

    return ret;
}
//Vector<float> StringUtils::split_floats_mk(const String &str,const Vector<String> &p_splitters, bool p_allow_empty) {

//    Vector<float> ret;
//    int from = 0;
//    int len = str.length();

//    while (true) {

//        int idx;
//        int end = StringUtils::findmk(str,p_splitters, from, &idx);
//        int spl_len = 1;
//        if (end < 0) {
//            end = len;
//        } else {
//            spl_len = p_splitters[idx].length();
//        }

//        if (p_allow_empty || (end > from)) {
//            ret.push_back(StringUtils::to_double(str.constData()+from));
//        }

//        if (end == len)
//            break;

//        from = end + spl_len;
//    }

//    return ret;
//}
PODVector<float> StringUtils::split_floats_mk(se_string_view str,se_string_view p_splitters, bool p_allow_empty) {

    PODVector<float> ret;
    size_t from = 0;
    size_t len = str.length();
    ret.reserve(str.size()/8); // just a ballpark to reduce number of reallocations.
    while (true) {

        auto end = str.find_first_of(p_splitters,from);
        if (end == String::npos) {
            end = len;
        }

        if (p_allow_empty || (end > from)) {
            ret.push_back(to_double(str.substr(from,end-from)));
        }

        if (end == len)
            break;

        from = end + 1;
    }

    return ret;
}

UIString StringUtils::join(const UIString &str,const PODVector<UIString> &parts) {
    UIString ret;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            ret += str;
        }
        ret += parts[i];
    }
    return ret;
}

CharType StringUtils::char_uppercase(CharType p_char) {

    return QChar::toUpper(p_char.unicode());
}

CharType StringUtils::char_lowercase(CharType p_char) {

    return QChar::toLower(p_char.unicode());
}
char StringUtils::char_lowercase(char p_char) {
    return CharToLower(p_char);
}
char StringUtils::char_uppercase(char p_char) {
    return CharToUpper(p_char);
}

String StringUtils::to_upper(se_string_view str){
    String res(str);
    res.make_upper();
    return res;
}
String StringUtils::to_lower(se_string_view str){
    String res(str);
    res.make_lower();
    return res;
}

String StringUtils::md5(const uint8_t *p_md5) {
    return hex_encode_buffer(p_md5, 16);
}

String StringUtils::hex_encode_buffer(const uint8_t *p_buffer, int p_len) {
    static const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

    String ret;
    ret.reserve(p_len*2);

    for (int i = 0; i < p_len; i++) {
        ret.push_back(hex[p_buffer[i] >> 4]);
        ret.push_back(hex[p_buffer[i] & 0xF]);
    }

    return ret;
}

String StringUtils::num(double p_num, int p_decimals) {

    if (p_decimals > 16)
        p_decimals = 16;

    char fmt[7];
    fmt[0] = '%';
    fmt[1] = '.';

    if (p_decimals < 0) {

        fmt[1] = 'l';
        fmt[2] = 'f';
        fmt[3] = 0;

    } else if (p_decimals < 10) {
        fmt[2] = '0' + p_decimals;
        fmt[3] = 'l';
        fmt[4] = 'f';
        fmt[5] = 0;
    } else {
        fmt[2] = '0' + (p_decimals / 10);
        fmt[3] = '0' + (p_decimals % 10);
        fmt[4] = 'l';
        fmt[5] = 'f';
        fmt[6] = 0;
    }
    char buf[256];

#if defined(__GNUC__) || defined(_MSC_VER)
    snprintf(buf, 256, fmt, p_num);
#else
    sprintf(buf, fmt, p_num);
#endif

    buf[255] = 0;
    //destroy trailing zeroes
    {

        bool period = false;
        int z = 0;
        while (buf[z]) {
            if (buf[z] == '.')
                period = true;
            z++;
        }

        if (period) {
            z--;
            while (z > 0) {

                if (buf[z] == '0') {

                    buf[z] = 0;
                } else if (buf[z] == '.') {

                    buf[z] = 0;
                    break;
                } else {

                    break;
                }

                z--;
            }
        }
    }

    return buf;
}

String StringUtils::num_int64(int64_t p_num, int base, bool capitalize_hex) {

    bool sign = p_num < 0;

    int64_t n = p_num;

    int chars = 0;
    do {
        n /= base;
        chars++;
    } while (n);

    if (sign)
        chars++;
    String s;
    s.resize(chars);
    char *c = s.data();

    n = p_num;
    do {
        int mod = ABS(n % base);
        if (mod >= 10) {
            char a = (capitalize_hex ? 'A' : 'a');
            c[--chars] = a + (mod - 10);
        } else {
            c[--chars] = '0' + mod;
        }

        n /= base;
    } while (n);

    if (sign)
        c[0] = '-';

    return s;
}

String StringUtils::num_uint64(uint64_t p_num, int base, bool capitalize_hex) {

    uint64_t n = p_num;

    int chars = 0;
    do {
        n /= base;
        chars++;
    } while (n);

    String s;
    s.resize(chars);
    char *c = s.data();
    n = p_num;
    do {
        int mod = n % base;
        if (mod >= 10) {
            char a = (capitalize_hex ? 'A' : 'a');
            c[--chars] = a + (mod - 10);
        } else {
            c[--chars] = '0' + mod;
        }

        n /= base;
    } while (n);

    return s;
}

String StringUtils::num_real(double p_num) {

    String s;
    String sd;
    /* integer part */

    bool neg = p_num < 0;
    p_num = ABS(p_num);
    int intn = (int)p_num;

    /* decimal part */

    if ((int)p_num != p_num) {

        double dec = p_num - (float)((int)p_num);

        int digit = 0;
        int decimals = MAX_DIGITS;

        int dec_int = 0;
        int dec_max = 0;

        while (true) {

            dec *= 10.0;
            dec_int = dec_int * 10 + (int)dec % 10;
            dec_max = dec_max * 10 + 9;
            digit++;

            if ((dec - (float)((int)dec)) < 1e-6)
                break;

            if (digit == decimals)
                break;
        }

        dec *= 10;
        int last = (int)dec % 10;

        if (last > 5) {
            if (dec_int == dec_max) {

                dec_int = 0;
                intn++;
            } else {

                dec_int++;
            }
        }

        String decimal;
        for (int i = 0; i < digit; i++) {

            char num[2] = { 0, 0 };
            num[0] = '0' + dec_int % 10;
            decimal = num + decimal;
            dec_int /= 10;
        }
        sd = '.' + decimal;
    } else {
        sd = ".0";
    }

    if (intn == 0)

        s = "0";
    else {
        while (intn) {

            char num = '0' + (intn % 10);
            intn /= 10;
            s = num + s;
        }
    }

    s = s + sd;
    if (neg)
        s = "-" + s;
    return s;
}

String StringUtils::num_scientific(double p_num) {
    return String(String::CtorSprintf(),"%g",p_num);
}

CharString StringUtils::ascii(const UIString &str,bool p_allow_extended) {
    if(p_allow_extended)
        return str.toLocal8Bit();
    else
        return str.toLatin1();
}

bool StringUtils::parse_utf8(UIString &str,const char *p_utf8, int p_len) {

    if (!p_utf8)
        return true;

    str = QString::fromUtf8(p_utf8, p_len);
    return !str.isEmpty();
}

String StringUtils::utf8(const UIString &str) {

    auto v=str.toUtf8();
    return String(v.data(),v.size());
}

int StringUtils::hex_to_int(const UIString &s,bool p_with_prefix) {

    if (p_with_prefix && s.length() < 3)
        return 0;
    QStringRef to_convert;
    if (p_with_prefix) {
        if (!begins_with(s,"0x"))
            return 0;
         to_convert = s.midRef(2);
    }
    else
        to_convert = s.midRef(0);
    return to_convert.toInt(nullptr,16);
}
int StringUtils::hex_to_int(se_string_view s,bool p_with_prefix) {

    if (p_with_prefix && s.length() < 3)
        return 0;
    se_string_view to_convert;
    if (p_with_prefix) {
        if (!begins_with(s,"0x"))
            return 0;
         to_convert = s.substr(2);
    }
    else
        to_convert = s;
    int res=0;
#ifndef __MINGW32__
    auto conv_res = std::from_chars(to_convert.begin(),to_convert.end(),res,16);
#else
    String zeroterm(to_convert);
    res = strtol(zeroterm.c_str(),nullptr,16);
#endif
    return res;
}
int64_t StringUtils::hex_to_int64(const UIString &s,bool p_with_prefix) {

    if (p_with_prefix && s.length() < 3)
        return 0;
    QStringRef to_convert;
    if (p_with_prefix) {
        if (!s.startsWith("0x"))
            return 0;
         to_convert = s.midRef(2);
    }
    else
        to_convert = s.midRef(0);
    return to_convert.toLongLong(nullptr,16);
}
int64_t StringUtils::hex_to_int64(se_string_view s,bool p_with_prefix) {

    if (p_with_prefix && s.length() < 3)
        return 0;
    se_string_view to_convert;
    if (p_with_prefix) {
        if (!s.starts_with("0x"))
            return 0;
         to_convert = s.substr(2);
    }
    else
        to_convert = s;
    int64_t v;
#ifndef __MINGW32__
    std::from_chars(to_convert.data(),to_convert.data()+to_convert.length(),v,16);
#else
    String zeroterm(to_convert);
    v = strtoll(zeroterm.c_str(),nullptr,16);
#endif
    return v;
}
int64_t StringUtils::bin_to_int64(const UIString &s,bool p_with_prefix) {

    if (p_with_prefix && s.length() < 3)
        return 0;
    QStringRef to_convert;
    if (p_with_prefix) {
        if (!s.startsWith("0b"))
            return 0;
         to_convert = s.midRef(2);
    }
    else
        to_convert = s.midRef(0);
    return to_convert.toLongLong(nullptr,2);
}
int64_t StringUtils::bin_to_int64(se_string_view s,bool p_with_prefix) {

    if (p_with_prefix && s.length() < 3)
        return 0;
    se_string_view to_convert;
    if (p_with_prefix) {
        if (!s.starts_with("0b"))
            return 0;
         to_convert = s.substr(2);
    }
    else
        to_convert = s;
    int64_t v;
#ifndef __MINGW32__
    std::from_chars(to_convert.data(),to_convert.data()+to_convert.length(),v,2);
#else
    String zeroterm(to_convert);
    v = strtoll(zeroterm.c_str(),nullptr,2);
#endif
    return v;
}
int64_t StringUtils::to_int64(const UIString &s) {
    return s.toLongLong();
}
int64_t StringUtils::to_int64(se_string_view s) {
    String tmp(s);
    return strtoll(tmp.c_str(),nullptr,0);
}
int StringUtils::to_int(const char *p_str, int p_len) {
    return QByteArray::fromRawData(p_str,p_len).toInt();
}
int StringUtils::to_int(se_string_view p_str) {
    return QByteArray::fromRawData(p_str.data(),p_str.length()).toInt();
}

bool StringUtils::is_numeric(const String &str) {

    if (str.length() == 0) {
        return false;
    }

    size_t s = 0;
    if (str[0] == '-')
        ++s;
    bool dot = false;
    for (size_t i = s; i < str.length(); i++) {

        char c = str[i];
        if (c == '.') {
            if (dot) {
                return false;
            }
            dot = true;
        }
        if (c < '0' || c > '9') {
            return false;
        }
    }

    return true; // TODO: Use the parser below for this instead
};

static double built_in_strtod(const CharType *string, /* A decimal ASCII floating-point number,
                 * optionally preceded by white space. Must
                 * have form "-I.FE-X", where I is the integer
                 * part of the mantissa, F is the fractional
                 * part of the mantissa, and X is the
                 * exponent. Either of the signs may be "+",
                 * "-", or omitted. Either I or F may be
                 * omitted, or both. The decimal point isn't
                 * necessary unless F is present. The "E" may
                 * actually be an "e". E and X may both be
                 * omitted (but not just one). */
        const CharType **endPtr = nullptr) /* If non-NULL, store terminating Cacter's
                 * address here. */
{

    static const int maxExponent = 511; /* Largest possible base 10 exponent.  Any
                     * exponent larger than this will already
                     * produce underflow or overflow, so there's
                     * no need to worry about additional digits.
                     */
    static const double powersOf10[] = { /* Table giving binary powers of 10.  Entry */
        10., /* is 10^2^i.  Used to convert decimal */
        100., /* exponents into floating-point numbers. */
        1.0e4,
        1.0e8,
        1.0e16,
        1.0e32,
        1.0e64,
        1.0e128,
        1.0e256
    };

    bool sign, expSign = false;
    double fraction, dblExp;
    const double *d;
    const CharType *p;
    CharType c;
    int exp = 0; /* Exponent read from "EX" field. */
    int fracExp = 0; /* Exponent that derives from the fractional
                 * part. Under normal circumstances, it is
                 * the negative of the number of digits in F.
                 * However, if I is very long, the last digits
                 * of I get dropped (otherwise a long I with a
                 * large negative exponent could cause an
                 * unnecessary overflow on I alone). In this
                 * case, fracExp is incremented one for each
                 * dropped digit. */
    int mantSize; /* Number of digits in mantissa. */
    int decPt; /* Number of mantissa digits BEFORE decimal
                 * point. */
    const CharType *pExp; /* Temporarily holds location of exponent in
                 * string. */

    /*
     * Strip off leading blanks and check for a sign.
     */

    p = string;
    while (*p == ' ' || *p == '\t' || *p == '\n') {
        p += 1;
    }
    if (*p == '-') {
        sign = true;
        p += 1;
    } else {
        if (*p == '+') {
            p += 1;
        }
        sign = false;
    }

    /*
     * Count the number of digits in the mantissa (including the decimal
     * point), and also locate the decimal point.
     */

    decPt = -1;
    for (mantSize = 0;; mantSize += 1) {
        c = *p;
        if (!c.isDigit()) {
            if ((c != '.') || (decPt >= 0)) {
                break;
            }
            decPt = mantSize;
        }
        p += 1;
    }

    /*
     * Now suck up the digits in the mantissa. Use two integers to collect 9
     * digits each (this is faster than using floating-point). If the mantissa
     * has more than 18 digits, ignore the extras, since they can't affect the
     * value anyway.
     */

    pExp = p;
    p -= mantSize;
    if (decPt < 0) {
        decPt = mantSize;
    } else {
        mantSize -= 1; /* One of the digits was the point. */
    }
    if (mantSize > 18) {
        fracExp = decPt - 18;
        mantSize = 18;
    } else {
        fracExp = decPt - mantSize;
    }
    if (mantSize == 0) {
        fraction = 0.0;
        p = string;
        goto done;
    } else {
        int frac1, frac2;

        frac1 = 0;
        for (; mantSize > 9; mantSize -= 1) {
            c = *p;
            p += 1;
            if (c == '.') {
                c = *p;
                p += 1;
            }
            frac1 = 10 * frac1 + c.digitValue();
        }
        frac2 = 0;
        for (; mantSize > 0; mantSize -= 1) {
            c = *p;
            p += 1;
            if (c == '.') {
                c = *p;
                p += 1;
            }
            frac2 = 10 * frac2 + c.digitValue();
        }
        fraction = (1.0e9 * frac1) + frac2;
    }

    /*
     * Skim off the exponent.
     */

    p = pExp;
    if ((*p == 'E') || (*p == 'e')) {
        p += 1;
        if (*p == '-') {
            expSign = true;
            p += 1;
        } else {
            if (*p == '+') {
                p += 1;
            }
            expSign = false;
        }
        if (!CharType(*p).isDigit()) {
            p = pExp;
            goto done;
        }
        while (CharType(*p).isDigit()) {
            exp = exp * 10 + p->digitValue();
            p += 1;
        }
    }
    if (expSign) {
        exp = fracExp - exp;
    } else {
        exp = fracExp + exp;
    }

    /*
     * Generate a floating-point number that represents the exponent. Do this
     * by processing the exponent one bit at a time to combine many powers of
     * 2 of 10. Then combine the exponent with the fraction.
     */

    if (exp < 0) {
        expSign = true;
        exp = -exp;
    } else {
        expSign = false;
    }

    if (exp > maxExponent) {
        exp = maxExponent;
        WARN_PRINT("Exponent too high")
    }
    dblExp = 1.0;
    for (d = powersOf10; exp != 0; exp >>= 1, ++d) {
        if (exp & 01) {
            dblExp *= *d;
        }
    }
    if (expSign) {
        fraction /= dblExp;
    } else {
        fraction *= dblExp;
    }

done:
    if (endPtr != nullptr) {
        *endPtr = (QChar *)p;
    }

    if (sign) {
        return -fraction;
    }
    return fraction;
}

double StringUtils::to_double(se_string_view p_str) {
    return QByteArray::fromRawData(p_str.data(),p_str.size()).toDouble();
}
double StringUtils::to_double(const char *p_str, char ** r_end) {
    return strtod(p_str,r_end);
}

double StringUtils::to_double(const CharType *p_str, const CharType **r_end) {

    return built_in_strtod(p_str, r_end);
}

int64_t StringUtils::to_int(const CharType *p_str, int p_len) {

    if (p_len == 0 || p_str[0].isNull())
        return 0;
    return QString::fromRawData(p_str,p_len).toLongLong();
}

String StringUtils::md5_text(const UIString &str) {

    String cs = to_utf8(str);
    unsigned char hash[16];
    CryptoCore::md5((unsigned char *)cs.data(), cs.length(), hash);
    return hex_encode_buffer(hash, 16);
}
String StringUtils::md5_text(se_string_view str) {
    unsigned char hash[16];
    CryptoCore::md5((unsigned char *)str.data(), str.length(), hash);
    return hex_encode_buffer(hash, 16);
}
String StringUtils::sha1_text(const UIString &str) {
    String cs = to_utf8(str);
    unsigned char hash[20];
    CryptoCore::sha1((unsigned char *)cs.data(), cs.length(), hash);
    return hex_encode_buffer(hash, 20);
}
String StringUtils::sha1_text(se_string_view str) {
    unsigned char hash[20];
    CryptoCore::sha1((unsigned char *)str.data(), str.length(), hash);
    return hex_encode_buffer(hash, 20);
}

//String StringUtils::sha256_text(const String &str) {
//    String cs = StringUtils::to_utf8(str);
//    unsigned char hash[32];
//    CryptoCore::sha256((unsigned char *)cs.data(), cs.length(), hash);
//    return StringUtils::hex_encode_buffer(hash, 32);
//}
String StringUtils::sha256_text(se_string_view cs) {
    unsigned char hash[32];
    CryptoCore::sha256((unsigned char *)cs.data(), cs.length(), hash);
    return hex_encode_buffer(hash, 32);
}

//Vector<uint8_t> StringUtils::md5_buffer(const String &str) {

//    String cs = StringUtils::to_utf8(str);
//    unsigned char hash[16];
//    CryptoCore::md5((unsigned char *)cs.data(), cs.length(), hash);

//    Vector<uint8_t> ret;
//    ret.resize(16);
//    for (int i = 0; i < 16; i++) {
//        ret.write[i] = hash[i];
//    }
//    return ret;
//};
PODVector<uint8_t> StringUtils::md5_buffer(se_string_view cs) {

    unsigned char hash[16];
    CryptoCore::md5((unsigned char *)cs.data(), cs.length(), hash);

    PODVector<uint8_t> ret;
    ret.resize(16);
    for (int i = 0; i < 16; i++) {
        ret[i] = hash[i];
    }
    return ret;
};

PODVector<uint8_t> StringUtils::sha1_buffer(const UIString &str) {
    String cs = to_utf8(str);
    PODVector<uint8_t> ret(20,0);
    CryptoCore::sha1((unsigned char *)cs.data(), cs.length(), ret.data());
    return ret;
}
PODVector<uint8_t> StringUtils::sha1_buffer(se_string_view cs) {
    PODVector<uint8_t> ret(20,0);
    CryptoCore::sha1((unsigned char *)cs.data(), cs.length(), ret.data());
    return ret;
}
PODVector<uint8_t> StringUtils::sha256_buffer(const UIString &str)  {
    String cs = to_utf8(str);
    PODVector<uint8_t> ret(32,0);
    CryptoCore::sha256((unsigned char *)cs.data(), cs.length(), ret.data());
    return ret;
}
PODVector<uint8_t> StringUtils::sha256_buffer(se_string_view str)  {
    PODVector<uint8_t> ret(32,0);
    CryptoCore::sha256((unsigned char *)str.data(), str.length(), ret.data());
    return ret;
}
UIString StringUtils::insert(const UIString &s,int p_at_pos, const UIString &p_string) {
    UIString res(s);
    res.insert(p_at_pos,p_string);
    return res;
}
String StringUtils::insert(se_string_view s,int p_at_pos, se_string_view p_string) {
    String res(s);
    res.insert(p_at_pos,String(p_string));
    return res;
}
UIString StringUtils::substr(const UIString &s,int p_from, int p_chars) {

    if(s.isEmpty())
        return s;
    if ((p_from + p_chars) > s.length()) {

        p_chars = s.length() - p_from;
    }
    return s.mid(p_from,p_chars);
}
se_string_view StringUtils::substr(se_string_view s,int p_from, size_t p_chars) {
    se_string_view res(s);
    if(s.empty())
        return res;
    ssize_t count = static_cast<ssize_t>(p_chars);
    if ((p_from + count) > ssize_t(s.length())) {

        p_chars = s.length() - p_from;
    }

    return res.substr(p_from,p_chars);
}

int StringUtils::find_last(const UIString &s,const UIString &p_str) {
    return s.lastIndexOf(p_str);
}
int StringUtils::find_last(const UIString &s,const CharType c) {
    return s.lastIndexOf(c);
}
size_t StringUtils::find_last(se_string_view s,char c) {
    return s.rfind(c);
}
size_t StringUtils::find_last(se_string_view s,se_string_view oth) {
    return s.rfind(oth);
}
int StringUtils::find(const UIString &s,const UIString &p_str, int p_from) {
    if (p_from < 0)
        return -1;
    return s.indexOf(p_str,p_from);
}

int StringUtils::find(const UIString &s,const char *p_str, int p_from) {

    if (p_from < 0)
        return -1;
    return s.indexOf(QLatin1String(p_str),p_from);
}
size_t StringUtils::find(se_string_view s,se_string_view p_str, size_t p_from) {

    return s.find(p_str,p_from);
}
size_t StringUtils::find(se_string_view s,char c, size_t p_from) {
    return s.find(c,p_from);
}

int StringUtils::findn(const UIString &s,const UIString &p_str, int p_from) {

    if (p_from < 0)
        return -1;
    return s.indexOf(p_str,p_from,Qt::CaseInsensitive);
}
size_t StringUtils::findn(se_string_view s,se_string_view p_str, int p_from) {

    if (p_from < 0)
        return String::npos;

    size_t src_len = p_str.length();

    if (src_len == 0 || s.empty() || src_len>s.length())
        return String::npos; // won't find anything!

    const char *srcd = s.data();

    for (size_t i = p_from; i <= (s.length() - src_len); i++) {

        bool found = true;
        for (size_t j = 0; j < src_len; j++) {

            size_t read_pos = i + j;

            if (read_pos >= s.length()) {

                ERR_PRINT("read_pos>=length()")
                return String::npos;
            }

            char src = CharToLower(srcd[read_pos]);
            char dst = CharToLower(p_str[j]);

            if (src != dst) {
                found = false;
                break;
            }
        }

        if (found)
            return i;
    }

    return String::npos;
}
int StringUtils::rfind(const UIString &s,const UIString &p_str, int p_from) {
    return s.lastIndexOf(p_str,p_from);
}
size_t StringUtils::rfind(se_string_view s,se_string_view p_str, int p_from) {
    return s.rfind(p_str,p_from);
}
size_t StringUtils::rfind(se_string_view s,char c, int p_from) {
    return s.rfind(c,p_from);
}
int StringUtils::rfindn(const UIString &s,const UIString &p_str, int p_from) {
    return s.lastIndexOf(p_str,p_from,Qt::CaseInsensitive);
}
size_t StringUtils::rfindn(se_string_view s,se_string_view p_str, int p_from) {
    QByteArray a(s.data(),s.size());
    QByteArray b(p_str.data(),p_str.size());
    int res = a.toLower().lastIndexOf(b.toLower());
    if(res==-1)
        return String::npos;

    return size_t(res);
}

bool StringUtils::ends_with(const UIString &s,const UIString &p_string) {
    return s.endsWith(p_string);
}
bool StringUtils::ends_with(const UIString &s,const char *p_string) {
    return s.endsWith(UIString::fromUtf8(p_string));
}

bool StringUtils::ends_with(const UIString &s,CharType p_char) {
    return s.endsWith(p_char);
}
bool StringUtils::ends_with(se_string_view s,se_string_view p_string) {
    return s.ends_with(p_string);
}
bool StringUtils::ends_with(se_string_view s,char c) {
    return s.ends_with(c);
}

bool StringUtils::begins_with(const UIString &str,CharType ch) {
    return str.startsWith(ch);
}
bool StringUtils::begins_with(const UIString &s,const UIString &p_string) {
    return s.startsWith(p_string);
}
bool StringUtils::begins_with(const UIString &s,const char *p_string) {
    return s.startsWith(UIString::fromUtf8(p_string));
}
bool StringUtils::begins_with(se_string_view s,se_string_view p_string) {
    if(s.size()<p_string.size())
        return false;
    return String::compare(s.begin(),s.begin()+p_string.size(),p_string.begin(),p_string.end())==0;
}

bool StringUtils::is_subsequence_of(const UIString &str,const UIString &p_string, Compare mode) {
    if(str.isEmpty())
        return true;
    return str.contains(p_string,mode==CaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
}
bool StringUtils::is_subsequence_of(se_string_view str,se_string_view p_string, Compare mode) {
    if(str.empty())
        return true;
    if(str.length()>p_string.length())
        return false;
    return is_subsequence_of(from_utf8(p_string),from_utf8(str),mode);
}
bool StringUtils::is_quoted(const UIString &str) {

    return is_enclosed_in(str,'"') || is_enclosed_in(str,'\'');
}
bool StringUtils::is_quoted(se_string_view str) {

    return is_enclosed_in(str,'"') || is_enclosed_in(str,'\'');
}
static int str_count(const UIString &s,const UIString &p_string, int p_from, int p_to, bool p_case_insensitive)  {
    if (p_string.isEmpty()) {
        return 0;
    }
    int len = s.length();
    int slen = p_string.length();
    if (len < slen) {
        return 0;
    }
    UIString str;
    if (p_from >= 0 && p_to >= 0) {
        if (p_to == 0) {
            p_to = len;
        } else if (p_from >= p_to) {
            return 0;
        }
        if (p_from == 0 && p_to == len) {
            str = s.mid(0,len);
        } else {
            str = s.mid(p_from, p_to - p_from);
        }
    } else {
        return 0;
    }
    int c = 0;
    int idx = -1;
    do {
        idx = p_case_insensitive ? StringUtils::findn(str,p_string) : StringUtils::find(str,p_string);
        if (idx != -1) {
            str = StringUtils::substr(str,idx + slen, str.length() - slen);
            ++c;
        }
    } while (idx != -1);
    return c;
}
static int str_count(se_string_view s,se_string_view p_string, int p_from, int p_to, bool p_case_insensitive)  {
    if (p_string.empty()) {
        return 0;
    }
    size_t len = s.length();
    size_t slen = p_string.length();
    if (len < slen) {
        return 0;
    }
    se_string_view str;
    if (p_from >= 0 && p_to >= 0) {
        if (p_to == 0) {
            p_to = len;
        } else if (p_from >= p_to) {
            return 0;
        }
        if (p_from == 0 && p_to == len) {
            str = s.substr(0,len);
        } else {
            str = s.substr(p_from, p_to - p_from);
        }
    } else {
        return 0;
    }
    int c = 0;
    auto idx = String::npos;
    do {
        idx = p_case_insensitive ? StringUtils::findn(str,p_string) : StringUtils::find(str,p_string);
        if (idx != String::npos) {
            str = StringUtils::substr(str,idx + slen, str.length() - slen);
            ++c;
        }
    } while (idx != String::npos);
    return c;
}

int StringUtils::count(const UIString &s,const UIString &p_string, int p_from, int p_to) {
    return str_count(s,p_string, p_from, p_to, false);
}

int StringUtils::countn(const UIString &s,const UIString &p_string, int p_from, int p_to) {

    return str_count(s,p_string, p_from, p_to, true);
}
int StringUtils::count(se_string_view s,se_string_view p_string, int p_from, int p_to) {
    return str_count(s,p_string, p_from, p_to, false);
}

int StringUtils::countn(se_string_view s,se_string_view p_string, int p_from, int p_to) {

    return str_count(s,p_string, p_from, p_to, true);
}

PODVector<se_string_view> StringUtils::bigrams(se_string_view str) {
    int n_pairs = str.length() - 1;
    PODVector<se_string_view> b;
    if (n_pairs <= 0) {
        return b;
    }
    b.resize(n_pairs);
    for (int i = 0; i < n_pairs; i++) {
        b[i] = substr(str,i, 2);
    }
    return b;
}

// Similarity according to Sorensen-Dice coefficient
float StringUtils::similarity(se_string_view lhs,se_string_view p_string) {
    if (lhs==p_string) {
        // Equal strings are totally similar
        return 1.0f;
    }
    if (lhs.length() < 2 || p_string.length() < 2) {
        // No way to calculate similarity without a single bigram
        return 0.0f;
    }

    PODVector<se_string_view> src_bigrams = bigrams(lhs);
    PODVector<se_string_view> tgt_bigrams = bigrams(p_string);

    int src_size = src_bigrams.size();
    int tgt_size = tgt_bigrams.size();

    float sum = src_size + tgt_size;
    float inter = 0;
    for (int i = 0; i < src_size; i++) {
        for (int j = 0; j < tgt_size; j++) {
            if (src_bigrams[i] == tgt_bigrams[j]) {
                inter++;
                break;
            }
        }
    }

    return (2.0f * inter) / sum;
}
static bool _wildcard_match(const CharType *p_pattern, const CharType *p_string, bool p_case_sensitive) {
    switch (p_pattern->toLatin1()) {
        case '\0':
            return p_string->isNull();
        case '*':
            return _wildcard_match(p_pattern + 1, p_string, p_case_sensitive) || (!p_string->isNull() && _wildcard_match(p_pattern, p_string + 1, p_case_sensitive));
        case '?':
            return !p_string->isNull() && (*p_string != '.') && _wildcard_match(p_pattern + 1, p_string + 1, p_case_sensitive);
        default:

            return (p_case_sensitive ? (*p_string == *p_pattern) : (p_string->toUpper() == p_pattern->toUpper())) && _wildcard_match(p_pattern + 1, p_string + 1, p_case_sensitive);
    }
}

static bool _wildcard_match(se_string_view p_pattern, se_string_view p_string, bool p_case_sensitive) {
    if(p_pattern.empty() && p_string.empty())
        return true;
    switch (p_pattern[0]) {
        case '*':
            return _wildcard_match(p_pattern.substr(1), p_string, p_case_sensitive) || (!p_string.empty() && _wildcard_match(p_pattern, p_string.substr(1), p_case_sensitive));
        case '?':
            return !p_string.empty() && (p_string[0] != '.') && _wildcard_match(p_pattern.substr(1), p_string.substr(1), p_case_sensitive);
        default:

            return (p_case_sensitive ? (p_string[0] == p_pattern[0]) : (CharToUpper(p_string[0]) == CharToUpper(p_pattern[0]))) && _wildcard_match(p_pattern.substr(1), p_string.substr(1), p_case_sensitive);
    }
}

bool StringUtils::match(const UIString &s, const UIString &p_wildcard, Compare sensitivity)  {

    if (p_wildcard.isEmpty() || s.isEmpty())
        return false;
    assert(sensitivity!=CaseNatural);
    return _wildcard_match(p_wildcard.constData(), s.constData(), sensitivity==CaseSensitive);
}
bool StringUtils::match(se_string_view s, se_string_view p_wildcard, Compare sensitivity)  {

    if (p_wildcard.empty() || s.empty())
        return false;
    assert(sensitivity!=CaseNatural);
    return _wildcard_match(p_wildcard, s, sensitivity==CaseSensitive);
}
bool StringUtils::matchn(const UIString &s,const UIString &p_wildcard)  {
    return match(s,p_wildcard,CaseInsensitive);
}
bool StringUtils::matchn(se_string_view s,se_string_view p_wildcard)  {
    return match(s,p_wildcard,CaseInsensitive);
}
String StringUtils::format(se_string_view fmt, const Variant &values) {

    static const String quote_char("\"");
    static const String underscore("_");
    String new_string(fmt);

    if (values.get_type() == VariantType::ARRAY) {
        Array values_arr = values;
        for (int i = 0; i < values_arr.size(); i++) {
            String i_as_str = num_int64(i);

            if (values_arr[i].get_type() == VariantType::ARRAY) { //Array in Array structure [["name","RobotGuy"],[0,"godot"],["strength",9000.91]]
                Array value_arr = values_arr[i];

                if (value_arr.size() == 2) {
                    Variant v_key = value_arr[0];
                    String key = v_key.as<String>();
                    if (quote_char == left(key,1) && quote_char == right(key,key.length() - 1)) {
                        key = substr(key,1, key.length() - 2);
                    }

                    Variant v_val = value_arr[1];
                    String val = v_val.as<String>();
                    if (is_enclosed_in(val,'"')) {
                        val = substr(val,1, val.length() - 2);
                    }
                    new_string.replace("{"+key+"}", val);
                } else {
                    ERR_PRINT("STRING.format Inner Array size != 2 ")
                }
            } else { //Array structure ["RobotGuy","Logis","rookie"]
                Variant v_val = values_arr[i];
                String val = v_val.as<String>();

                if (is_enclosed_in(val,'"')) {
                    val = substr(val,1, val.length() - 2);
                }

                new_string.replace("{"+i_as_str+"}", val);
            }
        }
    } else if (values.get_type() == VariantType::DICTIONARY) {
        Dictionary d = values;
        PODVector<Variant> keys(d.get_key_list());

        for (const Variant &E : keys) {
            String key(E.as<String>());
            String val(d[E].as<String>());

            if (is_enclosed_in(key,'"')) {
                key = substr(key,1, key.length() - 2);
            }

            if (is_enclosed_in(val,'"')) {
                val = substr(val,1, val.length() - 2);
            }

            new_string.replace("{"+key+"}", val);
        }
    } else {
        ERR_PRINT("Invalid type: use Array or Dictionary.")
    }

    return new_string;
}

UIString StringUtils::replace_first(const UIString &s,const UIString &p_key, const UIString &p_with)  {

    int pos = find(s,p_key);
    UIString res=s;
    if(pos<0)
        return res;
    return res.replace(pos,p_key.length(),p_with);
}
String StringUtils::replace_first(se_string_view s,se_string_view p_key, se_string_view p_with)  {

    auto pos = find(s,p_key);
    String res(s);
    if(pos==String::npos)
        return res;
    res.replace(pos,p_key.length(),p_with.data(),p_with.size());
    return res;
}
UIString StringUtils::replacen(const UIString &s,const UIString &p_key, const UIString &p_with) {
    QString new_string=s;
    return new_string.replace(p_key,p_with,Qt::CaseInsensitive);
}
String StringUtils::replacen(se_string_view s,se_string_view p_key, se_string_view p_with) {
    QString new_string=from_utf8(s);
    return to_utf8(new_string.replace(from_utf8(p_key),from_utf8(p_with),Qt::CaseInsensitive));
}
String StringUtils::replace(se_string_view str,se_string_view p_key, se_string_view p_with) {
    return String(str).replaced(p_key,p_with);
}
String StringUtils::replace(se_string_view str,char p_key, char p_with) {
    return String(str).replaced(p_key,p_with);
}

void StringUtils::Inplace::replace(UIString &s,int i,int len, const UIString &p_after) {
    s.replace(i,len,p_after);
}

UIString StringUtils::repeat(const UIString &str,int p_count) {

    ERR_FAIL_COND_V_MSG(p_count < 0, UIString(), "Parameter count should be a positive number.")

    UIString new_string;

    new_string.resize(str.length() * p_count + 1);

    for (int i = 0; i < p_count; i++)
        for (int j = 0; j < str.length(); j++)
            new_string[i * str.length() + j] = str[j];

    return new_string;
}
String StringUtils::repeat(se_string_view str,int p_count) {

    ERR_FAIL_COND_V_MSG(p_count < 0, String(), "Parameter count should be a positive number.")

    String new_string;

    new_string.reserve(p_count*str.size());
    for(int i=0; i<p_count; ++i)
        new_string.append(str);

    return new_string;
}
UIString StringUtils::left(const UIString &s,int p_pos) {
    return s.mid(0, p_pos);
}
se_string_view StringUtils::left(se_string_view s,int p_pos) {
    return se_string_view(s).substr(0, p_pos);
}

UIString StringUtils::right(const UIString &s,int p_pos){

    return s.mid(p_pos);
}
se_string_view StringUtils::right(se_string_view s,int p_pos){

    return s.substr(p_pos);
}
CharType StringUtils::ord_at(const UIString &str,int p_idx) {

    ERR_FAIL_INDEX_V(p_idx, str.length(), 0)
    return str[p_idx];
}

//String StringUtils::dedent(const String &str) {

//    String new_string;
//    String indent;
//    bool has_indent = false;
//    bool has_text = false;
//    int line_start = 0;
//    int indent_stop = -1;

//    for (int i = 0; i < str.length(); i++) {

//        CharType c = str[i];
//        if (c == '\n') {
//            if (has_text)
//                new_string += StringUtils::substr(str,indent_stop, i - indent_stop);
//            new_string += '\n';
//            has_text = false;
//            line_start = i + 1;
//            indent_stop = -1;
//        } else if (!has_text) {
//            if (c > 32) {
//                has_text = true;
//                if (!has_indent) {
//                    has_indent = true;
//                    indent = StringUtils::substr(str,line_start, i - line_start);
//                    indent_stop = i;
//                }
//            }
//            if (has_indent && indent_stop < 0) {
//                int j = i - line_start;
//                if (j >= indent.length() || c != indent[j])
//                    indent_stop = i;
//            }
//        }
//    }

//    if (has_text)
//        new_string += StringUtils::substr(str,indent_stop);

//    return String(new_string);
//}
String StringUtils::dedent(se_string_view str) {

    String new_string;
    String indent;
    bool has_indent = false;
    bool has_text = false;
    size_t line_start = 0;
    int indent_stop = -1;

    for (size_t i = 0; i < str.length(); i++) {

        char c = str[i];
        if (c == '\n') {
            if (has_text)
                new_string += substr(str,indent_stop, i - indent_stop);
            new_string += '\n';
            has_text = false;
            line_start = i + 1;
            indent_stop = -1;
        } else if (!has_text) {
            if (c > 32) {
                has_text = true;
                if (!has_indent) {
                    has_indent = true;
                    indent = substr(str,line_start, i - line_start);
                    indent_stop = i;
                }
            }
            if (has_indent && indent_stop < 0) {
                int j = i - line_start;
                if (j >= indent.length() || c != indent[j])
                    indent_stop = i;
            }
        }
    }

    if (has_text)
        new_string += substr(str,indent_stop);

    return new_string;
}
UIString StringUtils::strip_edges(const UIString &str,bool left, bool right)  {

    int len = str.length();
    int beg = 0, end = len;

    if (left) {
        for (int i = 0; i < len; i++) {

            if (str[i] <= 32)
                beg++;
            else
                break;
        }
    }

    if (right) {
        for (int i = (int)(len - 1); i >= 0; i--) {

            if (str[i] <= 32)
                end--;
            else
                break;
        }
    }

    if (beg == 0 && end == len)
        return str;

    return substr(str,beg, end - beg);
}
se_string_view StringUtils::strip_edges(se_string_view str,bool left, bool right)  {

    int len = str.length();
    int beg = 0, end = len;

    if (left) {
        for (int i = 0; i < len; i++) {

            if (str[i] <= 32)
                beg++;
            else
                break;
        }
    }

    if (right) {
        for (int i = (int)(len - 1); i >= 0; i--) {

            if (str[i] <= 32)
                end--;
            else
                break;
        }
    }

    if (beg == 0 && end == len)
        return str;

    return substr(str,beg, end - beg);
}

UIString StringUtils::strip_escapes(const UIString &str)  {

    QString new_string;
    for (int i = 0; i < str.length(); i++) {

        // Escape characters on first page of the ASCII table, before 32 (Space).
        if (str[i] < 32)
            continue;
        new_string += str[i];
    }

    return UIString(new_string);
}
String StringUtils::strip_escapes(se_string_view str)  {

    String new_string;
    for (size_t i = 0; i < str.length(); i++) {

        // Escape characters on first page of the ASCII table, before 32 (Space).
        if (str[i] < 32)
            continue;
        new_string += str[i];
    }

    return new_string;
}

UIString StringUtils::lstrip(const UIString &str,const UIString &p_chars)  {

    int len = str.length();
    int beg;

    for (beg = 0; beg < len; beg++) {

        if (find_char(p_chars,str[beg]) == -1)
            break;
    }

    if (beg == 0)
        return str;

    return substr(str,beg, len - beg);
}
se_string_view StringUtils::lstrip(se_string_view str,se_string_view p_chars)  {

    size_t len = str.length();
    size_t beg;

    for (beg = 0; beg < len; beg++) {

        if (find_char(p_chars,str[beg]) == String::npos)
            break;
    }

    if (beg == 0)
        return str;

    return substr(str,beg, len - beg);
}

UIString StringUtils::rstrip(const UIString &str,const UIString &p_chars)  {

    int len = str.length();
    int end;

    for (end = len - 1; end >= 0; end--) {

        if (find_char(p_chars,str[end]) == -1)
            break;
    }

    if (end == len - 1)
        return str;

    return substr(str,0, end + 1);
}
se_string_view StringUtils::rstrip(se_string_view str,se_string_view p_chars)  {

    int len = str.length();
    int end;

    for (end = len - 1; end >= 0; end--) {

        if (find_char(p_chars,str[end]) == se_string_view::npos)
            break;
    }

    if (end == len - 1)
        return str;

    return substr(str,0, end + 1);
}
String PathUtils::simplify_path(se_string_view str) {

    String s(str);
    String drive;
    if (s.starts_with("local://")) {
        drive = "local://";
        s = s.substr(8);
    } else if (s.starts_with("res://")) {

        drive = "res://";
        s = s.substr(6);
    } else if (s.starts_with("user://")) {

        drive = "user://";
        s = s.substr(7, s.length());
    } else if (s.starts_with("/") || s.starts_with("\\")) {

        drive = s.substr(0, 1);
        s = s.substr(1);
    } else {

        size_t p = s.find(":/");
        if (p == String::npos)
            p = s.find(":\\");
        if (p != String::npos && p < s.find("/")) {

            drive = s.substr(0, p + 2);
            s = s.substr(p + 2, s.length());
        }
    }

    s.replace('\\', '/');
    while (true) { // in case of using 2 or more slash
        String compare =s.replaced("//", "/");
        if (s == compare)
            break;
        else
            s = compare;
    }
    FixedVector<se_string_view,16,true> dirs;
    FixedVector<se_string_view,16,true> filtered;
    String::split_ref(dirs,s,'/');

    for (se_string_view d : dirs) {
        if (d == "."_sv) {
            continue;
        } else if (d == ".."_sv) {
            if(filtered.empty())
                continue;
            filtered.pop_back(); // remove pre
        }
        else
            filtered.push_back(d);
    }

    return drive + String::joined(filtered,"/");
}
static int _humanize_digits(int p_num) {

    if (p_num < 100)
        return 2;
    else if (p_num < 1024)
        return 1;
    else
        return 0;
}

//String PathUtils::humanize_size(uint64_t p_size) {

//    uint64_t _div = 1;
//    static const char *prefix[] = { (" B"), (" KiB"), (" MiB"), (" GiB"), (" TiB"), (" PiB"), (" EiB"), nullptr };
//    int prefix_idx = 0;

//    while (p_size > (_div * 1024) && prefix[prefix_idx]) {
//        _div *= 1024;
//        prefix_idx++;
//    }

//    int digits = prefix_idx > 0 ? _humanize_digits(p_size / _div) : 0;
//    double divisor = prefix_idx > 0 ? _div : 1;

//    return StringUtils::pad_decimals(String::number(p_size / divisor),digits) + StringUtils::from_utf8(RTR(prefix[prefix_idx]));
//}
String PathUtils::humanize_size(uint64_t p_size) {

    uint64_t _div = 1;
    static const char *prefix[] = { (" B"), (" KiB"), (" MiB"), (" GiB"), (" TiB"), (" PiB"), (" EiB"), nullptr };
    int prefix_idx = 0;

    while (p_size > (_div * 1024) && prefix[prefix_idx]) {
        _div *= 1024;
        prefix_idx++;
    }

    int digits = prefix_idx > 0 ? _humanize_digits(p_size / _div) : 0;
    double divisor = prefix_idx > 0 ? _div : 1;

    return StringUtils::pad_decimals(StringUtils::num(p_size / divisor),digits) + (RTR(prefix[prefix_idx]));
}

bool PathUtils::is_abs_path(const UIString &str) {

    if (str.length() > 1)
        return (str[0] == '/' || str[0] == '\\' || str.contains(":/") || str.contains(":\\"));
    else if (str.length() == 1)
        return (str[0] == '/' || str[0] == '\\');
    else
        return false;
}
bool PathUtils::is_abs_path(se_string_view str) {

    if (str.length() > 1)
        return (str[0] == '/' || str[0] == '\\' || str.contains(":/") || str.contains(":\\"));
    else if (str.length() == 1)
        return (str[0] == '/' || str[0] == '\\');
    else
        return false;
}
bool StringUtils::is_valid_identifier(const UIString &str) {

    int len =str.length();

    if (len == 0)
        return false;

    for (int i = 0; i < len; i++) {

        if (i == 0) {
            if (str.front() >= '0' && str.front() <= '9')
                return false; // no start with number plz
        }
        QChar c = str[i];
        bool valid_char = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';

        if (!valid_char)
            return false;
    }

    return true;
}
bool StringUtils::is_valid_identifier(se_string_view str) {

    int len =str.length();

    if (len == 0)
        return false;

    for (int i = 0; i < len; i++) {

        if (i == 0) {
            if (str.front() >= '0' && str.front() <= '9')
                return false; // no start with number plz
        }
        char c = str[i];
        bool valid_char = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';

        if (!valid_char)
            return false;
    }

    return true;
}
//kind of poor should be rewritten properly

UIString StringUtils::word_wrap(const UIString &str,int p_chars_per_line) {

    int from = 0;
    int last_space = 0;
    UIString ret;
    for (int i = 0; i < str.length(); i++) {
        if (i - from >= p_chars_per_line) {
            if (last_space == -1) {
                ret += substr(str,from, i - from + 1) + "\n";
            } else {
                ret += substr(str,from, last_space - from) + "\n";
                i = last_space; //rewind
            }
            from = i + 1;
            last_space = -1;
        } else if (str[i] == ' ' || str[i] == '\t') {
            last_space = i;
        } else if (str[i] == '\n') {
            ret += substr(str,from, i - from) + "\n";
            from = i + 1;
            last_space = -1;
        }
    }

    if (from < str.length()) {
        ret += substr(str,from);
    }

    return UIString(ret);
}

//String StringUtils::http_escape(const String &s) {
//    const CharString temp = s.toUtf8();
//    QString res;
//    for (char ord : temp) {
//        if (ord == '.' || ord == '-' || ord == '_' || ord == '~' ||
//                (ord >= 'a' && ord <= 'z') ||
//                (ord >= 'A' && ord <= 'Z') ||
//                (ord >= '0' && ord <= '9')) {
//            res += ord;
//        } else {
//            char h_Val[3];
//#if defined(__GNUC__) || defined(_MSC_VER)
//            snprintf(h_Val, 3, "%hhX", ord);
//#else
//            sprintf(h_Val, "%hhX", ord);
//#endif
//            res += "%";
//            res += h_Val;
//        }
//    }
//    return String(res);
//}
String StringUtils::http_escape(se_string_view temp) {
    String res;
    for (char ord : temp) {
        if (ord == '.' || ord == '-' || ord == '_' || ord == '~' ||
                (ord >= 'a' && ord <= 'z') ||
                (ord >= 'A' && ord <= 'Z') ||
                (ord >= '0' && ord <= '9')) {
            res += ord;
        } else {
            char h_Val[3];
#if defined(__GNUC__) || defined(_MSC_VER)
            snprintf(h_Val, 3, "%hhX", ord);
#else
            sprintf(h_Val, "%hhX", ord);
#endif
            res += "%";
            res += h_Val;
        }
    }
    return res;
}
//String StringUtils::http_unescape(const String &str) {
//    QString res;
//    for (int i = 0; i < str.length(); ++i) {
//        if (str.at(i) == '%' && i + 2 < str.length()) {
//            CharType ord1 = str.at(i + 1);
//            if ((ord1 >= '0' && ord1 <= '9') || (ord1 >= 'A' && ord1 <= 'Z')) {
//                CharType ord2 = str.at(i + 2);
//                if ((ord2 >= '0' && ord2 <= '9') || (ord2 >= 'A' && ord2 <= 'Z')) {
//                    char bytes[3] = { (char)ord1.toLatin1(), (char)ord2.toLatin1(), 0 };
//                    res += (char)strtol(bytes, nullptr, 16);
//                    i += 2;
//                }
//            } else {
//                res += str.at(i);
//            }
//        } else {
//            res += str.at(i);
//        }
//    }
//    return StringUtils::from_utf8(StringUtils::ascii(res));
//}
String StringUtils::http_unescape(se_string_view str) {
    String res;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str.at(i) == '%' && i + 2 < str.length()) {
            char ord1 = str.at(i + 1);
            if ((ord1 >= '0' && ord1 <= '9') || (ord1 >= 'A' && ord1 <= 'Z')) {
                char ord2 = str.at(i + 2);
                if ((ord2 >= '0' && ord2 <= '9') || (ord2 >= 'A' && ord2 <= 'Z')) {
                    char bytes[3] = { (char)ord1, (char)ord2, 0 };
                    res += (char)strtol(bytes, nullptr, 16);
                    i += 2;
                }
            } else {
                res += str.at(i);
            }
        } else {
            res += str.at(i);
        }
    }
    return res;
}
//String StringUtils::c_unescape(const String &str) {

//    String escaped = str;
//    escaped = escaped.replace("\\a", "\a");
//    escaped = escaped.replace("\\b", "\b");
//    escaped = escaped.replace("\\f", "\f");
//    escaped = escaped.replace("\\n", "\n");
//    escaped = escaped.replace("\\r", "\r");
//    escaped = escaped.replace("\\t", "\t");
//    escaped = escaped.replace("\\v", "\v");
//    escaped = escaped.replace("\\'", "\'");
//    escaped = escaped.replace("\\\"", "\"");
//    escaped = escaped.replace("\\?", "\?");
//    escaped = escaped.replace("\\\\", "\\");

//    return escaped;
//}
String StringUtils::c_unescape(se_string_view str) {

    String escaped(str);
    escaped = StringUtils::replace(escaped,"\\a", "\a");
    escaped = StringUtils::replace(escaped,"\\b", "\b");
    escaped = StringUtils::replace(escaped,"\\f", "\f");
    escaped = StringUtils::replace(escaped,"\\n", "\n");
    escaped = StringUtils::replace(escaped,"\\r", "\r");
    escaped = StringUtils::replace(escaped,"\\t", "\t");
    escaped = StringUtils::replace(escaped,"\\v", "\v");
    escaped = StringUtils::replace(escaped,"\\'", "\'");
    escaped = StringUtils::replace(escaped,"\\\"", "\"");
    escaped = StringUtils::replace(escaped,"\\?", "\?");
    escaped = StringUtils::replace(escaped,"\\\\", "\\");

    return escaped;
}
//String StringUtils::c_escape(const String &e) {

//    String escaped = e;
//    escaped = escaped.replace("\\", "\\\\");
//    escaped = escaped.replace("\a", "\\a");
//    escaped = escaped.replace("\b", "\\b");
//    escaped = escaped.replace("\f", "\\f");
//    escaped = escaped.replace("\n", "\\n");
//    escaped = escaped.replace("\r", "\\r");
//    escaped = escaped.replace("\t", "\\t");
//    escaped = escaped.replace("\v", "\\v");
//    escaped = escaped.replace("\'", "\\'");
//    escaped = escaped.replace("\?", "\\?");
//    escaped = escaped.replace("\"", "\\\"");

//    return escaped;
//}
String StringUtils::c_escape(se_string_view e) {

    String escaped(e);
    escaped = StringUtils::replace(escaped,"\\", "\\\\");
    escaped = StringUtils::replace(escaped,"\a", "\\a");
    escaped = StringUtils::replace(escaped,"\b", "\\b");
    escaped = StringUtils::replace(escaped,"\f", "\\f");
    escaped = StringUtils::replace(escaped,"\n", "\\n");
    escaped = StringUtils::replace(escaped,"\r", "\\r");
    escaped = StringUtils::replace(escaped,"\t", "\\t");
    escaped = StringUtils::replace(escaped,"\v", "\\v");
    escaped = StringUtils::replace(escaped,"\'", "\\'");
    escaped = StringUtils::replace(escaped,"\?", "\\?");
    escaped = StringUtils::replace(escaped,"\"", "\\\"");

    return escaped;
}

//String StringUtils::c_escape_multiline(const String &str) {

//    String escaped = str;
//    escaped = escaped.replace("\\", "\\\\");
//    escaped = escaped.replace("\"", "\\\"");

//    return escaped;
//}
String StringUtils::c_escape_multiline(se_string_view str) {

    String escaped(str);
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");

    return escaped;
}
//String StringUtils::json_escape(const String &str) {

//    String escaped = str;
//    escaped = escaped.replace("\\", "\\\\");
//    escaped = escaped.replace("\b", "\\b");
//    escaped = escaped.replace("\f", "\\f");
//    escaped = escaped.replace("\n", "\\n");
//    escaped = escaped.replace("\r", "\\r");
//    escaped = escaped.replace("\t", "\\t");
//    escaped = escaped.replace("\v", "\\v");
//    escaped = escaped.replace("\"", "\\\"");

//    return escaped;
//}
String StringUtils::json_escape(se_string_view str) {

    String escaped(str);
    escaped.replace("\\", "\\\\");
    escaped.replace("\b", "\\b");
    escaped.replace("\f", "\\f");
    escaped.replace("\n", "\\n");
    escaped.replace("\r", "\\r");
    escaped.replace("\t", "\\t");
    escaped.replace("\v", "\\v");
    escaped.replace("\"", "\\\"");

    return escaped;
}
UIString StringUtils::xml_escape(const UIString &arg,bool p_escape_quotes)  {

    UIString str = arg;
    str = str.replace("&", "&amp;");
    str = str.replace("<", "&lt;");
    str = str.replace(">", "&gt;");
    if (p_escape_quotes) {
        str = str.replace("'", "&apos;");
        str = str.replace("\"", "&quot;");
    }
    return str;
}
String StringUtils::xml_escape(se_string_view arg,bool p_escape_quotes)  {

    String str(arg);
    str = StringUtils::replace(str,"&", "&amp;");
    str = StringUtils::replace(str,"<", "&lt;");
    str = StringUtils::replace(str,">", "&gt;");
    if (p_escape_quotes) {
        str = StringUtils::replace(str,"'", "&apos;");
        str = StringUtils::replace(str,"\"", "&quot;");
    }
    return str;
}
static _FORCE_INLINE_ int _xml_unescape(const CharType *p_src, int p_src_len, CharType *p_dst) {

    int len = 0;
    while (p_src_len) {

        if (*p_src == '&') {

            int eat = 0;

            if (p_src_len >= 4 && p_src[1] == '#') {

                uint16_t c = 0;

                for (int i = 2; i < p_src_len; i++) {

                    eat = i + 1;
                    CharType ct = p_src[i];
                    int ct_v;
                    if (ct == ';') {
                        break;
                    } else if (ct >= '0' && ct <= '9') {
                        ct_v = ct.digitValue();
                    } else if (ct >= 'a' && ct <= 'f') {
                        ct_v = (ct.toLatin1() - 'a') + 10;
                    } else if (ct >= 'A' && ct <= 'F') {
                        ct_v = (ct.toLatin1() - 'A') + 10;
                    } else {
                        continue;
                    }
                    c <<= 4;
                    c |= ct_v;
                }

                if (p_dst)
                    *p_dst = QChar(c);

            } else if (p_src_len >= 4 && p_src[1] == 'g' && p_src[2] == 't' && p_src[3] == ';') {

                if (p_dst)
                    *p_dst = '>';
                eat = 4;
            } else if (p_src_len >= 4 && p_src[1] == 'l' && p_src[2] == 't' && p_src[3] == ';') {

                if (p_dst)
                    *p_dst = '<';
                eat = 4;
            } else if (p_src_len >= 5 && p_src[1] == 'a' && p_src[2] == 'm' && p_src[3] == 'p' && p_src[4] == ';') {

                if (p_dst)
                    *p_dst = '&';
                eat = 5;
            } else if (p_src_len >= 6 && p_src[1] == 'q' && p_src[2] == 'u' && p_src[3] == 'o' && p_src[4] == 't' && p_src[5] == ';') {

                if (p_dst)
                    *p_dst = '"';
                eat = 6;
            } else if (p_src_len >= 6 && p_src[1] == 'a' && p_src[2] == 'p' && p_src[3] == 'o' && p_src[4] == 's' && p_src[5] == ';') {

                if (p_dst)
                    *p_dst = '\'';
                eat = 6;
            } else {

                if (p_dst)
                    *p_dst = *p_src;
                eat = 1;
            }

            if (p_dst)
                p_dst++;

            len++;
            p_src += eat;
            p_src_len -= eat;
        } else {

            if (p_dst) {
                *p_dst = *p_src;
                p_dst++;
            }
            len++;
            p_src++;
            p_src_len--;
        }
    }

    return len;
}
static int _xml_unescape(const char *p_src, int p_src_len, char *p_dst) {

    int len = 0;
    while (p_src_len) {

        if (*p_src == '&') {

            int eat = 0;

            if (p_src_len >= 4 && p_src[1] == '#') {

                char c = 0;

                for (int i = 2; i < p_src_len; i++) {

                    eat = i + 1;
                    char ct = p_src[i];
                    int ct_v;
                    if (ct == ';') {
                        break;
                    } else if (ct >= '0' && ct <= '9') {
                        ct_v = ct-'0';
                    } else if (ct >= 'a' && ct <= 'f') {
                        ct_v = (ct - 'a') + 10;
                    } else if (ct >= 'A' && ct <= 'F') {
                        ct_v = (ct - 'A') + 10;
                    } else {
                        continue;
                    }
                    c <<= 4;
                    c |= ct_v;
                }

                if (p_dst)
                    *p_dst = c;

            } else if (p_src_len >= 4 && p_src[1] == 'g' && p_src[2] == 't' && p_src[3] == ';') {

                if (p_dst)
                    *p_dst = '>';
                eat = 4;
            } else if (p_src_len >= 4 && p_src[1] == 'l' && p_src[2] == 't' && p_src[3] == ';') {

                if (p_dst)
                    *p_dst = '<';
                eat = 4;
            } else if (p_src_len >= 5 && p_src[1] == 'a' && p_src[2] == 'm' && p_src[3] == 'p' && p_src[4] == ';') {

                if (p_dst)
                    *p_dst = '&';
                eat = 5;
            } else if (p_src_len >= 6 && p_src[1] == 'q' && p_src[2] == 'u' && p_src[3] == 'o' && p_src[4] == 't' && p_src[5] == ';') {

                if (p_dst)
                    *p_dst = '"';
                eat = 6;
            } else if (p_src_len >= 6 && p_src[1] == 'a' && p_src[2] == 'p' && p_src[3] == 'o' && p_src[4] == 's' && p_src[5] == ';') {

                if (p_dst)
                    *p_dst = '\'';
                eat = 6;
            } else {

                if (p_dst)
                    *p_dst = *p_src;
                eat = 1;
            }

            if (p_dst)
                p_dst++;

            len++;
            p_src += eat;
            p_src_len -= eat;
        } else {

            if (p_dst) {
                *p_dst = *p_src;
                p_dst++;
            }
            len++;
            p_src++;
            p_src_len--;
        }
    }

    return len;
}
UIString StringUtils::xml_unescape(const UIString &arg) {

    UIString str;
    int l = arg.length();
    int len = _xml_unescape(arg.constData(), l, nullptr);
    if (len == 0)
        return UIString();
    str.resize(len);
    _xml_unescape(arg.constData(), l, str.data());

    return str;
}
String StringUtils::xml_unescape(se_string_view arg) {

    String str;
    int l = arg.length();
    int len = _xml_unescape(arg.data(), l, nullptr);
    if (len == 0)
        return String();
    str.resize(len);
    _xml_unescape(arg.data(), l, str.data());

    return str;
}
//String StringUtils::pad_decimals(const String &str,int p_digits) {

//    String s = str;
//    int c = s.indexOf(".");

//    if (c == -1) {
//        if (p_digits <= 0) {
//            return s;
//        }
//        s += '.';
//        c = s.length() - 1;
//    } else {
//        if (p_digits <= 0) {
//            return StringUtils::substr(s,0, c);
//        }
//    }

//    if (s.length() - (c + 1) > p_digits) {
//        s = StringUtils::substr(s, 0, c + p_digits + 1);
//    } else {
//        while (s.length() - (c + 1) < p_digits) {
//            s += '0';
//        }
//    }
//    return s;
//}
String StringUtils::pad_decimals(se_string_view str,int p_digits) {

    String s(str);
    auto c = s.find('.');

    if (c == String::npos) {
        if (p_digits <= 0) {
            return s;
        }
        s += '.';
        c = s.length() - 1;
    } else {
        if (p_digits <= 0) {
            return String(substr(s,0, c));
        }
    }

    if (s.length() - (c + 1) > p_digits) {
        s = substr(s, 0, c + p_digits + 1);
    } else {
        while (s.length() - (c + 1) < p_digits) {
            s += '0';
        }
    }
    return s;
}

UIString StringUtils::pad_zeros(const UIString &src,int p_digits) {

    UIString s = src;
    int end = find(s,".");

    if (end == -1) {
        end = s.length();
    }

    if (end == 0)
        return s;

    int begin = 0;

    while (begin < end && (s[begin] < '0' || s[begin] > '9')) {
        begin++;
    }

    if (begin >= end)
        return s;

    while (end - begin < p_digits) {

        s = s.insert(begin, "0");
        end++;
    }

    return s;
}
String StringUtils::pad_zeros(se_string_view src,int p_digits) {

    String s(src);
    auto end = s.find('.');

    if (end == String::npos) {
        end = s.length();
    }

    if (end == 0)
        return s;

    size_t begin = 0;

    while (begin < end && (s[begin] < '0' || s[begin] > '9')) {
        begin++;
    }

    if (begin >= end)
        return s;

    while (end - begin < size_t(p_digits)) {

        s = s.insert(begin, "0");
        end++;
    }

    return s;
}

UIString StringUtils::trim_prefix(const UIString &src,const UIString &p_prefix) {

    UIString s = src;
    if (begins_with(s,p_prefix)) {
        return substr(s,p_prefix.length(), s.length() - p_prefix.length());
    }
    return s;
}
se_string_view StringUtils::trim_prefix(se_string_view src,se_string_view p_prefix) {

    se_string_view s = src;
    if (begins_with(s,p_prefix)) {
        return substr(s,p_prefix.length(), s.length() - p_prefix.length());
    }
    return s;
}
UIString StringUtils::trim_suffix(const UIString &src,const UIString &p_suffix) {

    UIString s = src;
    if (ends_with(s,p_suffix)) {
        return substr(s,0, s.length() - p_suffix.length());
    }
    return s;
}
se_string_view StringUtils::trim_suffix(se_string_view src,se_string_view p_suffix) {

    se_string_view s = src;
    if (ends_with(s,p_suffix)) {
        return substr(s,0, s.length() - p_suffix.length());
    }
    return s;
}

bool StringUtils::is_valid_integer(const UIString &str) {

    int len = str.length();

    if (len == 0)
        return false;

    int from = 0;
    if (len != 1 && (str.front() == '+' || str.front() == '-'))
        from++;

    for (int i = from; i < len; i++) {

        if (!str[i].isDigit())
            return false; // no start with number plz
    }

    return true;
}
bool StringUtils::is_valid_integer(se_string_view str) {

    int len = str.length();

    if (len == 0)
        return false;

    int from = 0;
    if (len != 1 && (str[0] == '+' || str[0] == '-'))
        from++;

    for (int i = from; i < len; i++) {
        if (isdigit(str[i])==0)
            return false; // no start with number plz
    }

    return true;
}
bool StringUtils::is_valid_html_color(se_string_view clr) {

    return Color::html_is_valid(clr);
}
bool StringUtils::is_valid_hex_number(se_string_view str,bool p_with_prefix) {

    size_t len = str.length();

    if (len == 0)
        return false;

    size_t from = 0;
    if (len != 1 && (str.front() == '+' || str.front() == '-'))
        from++;

    if (p_with_prefix) {

        if (len < 3)
            return false;
        if (str[from] != '0' || str[from + 1] != 'x') {
            return false;
        }
        from += 2;
    }

    for (size_t i = from; i < len; i++) {

        char c = str[i];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
            continue;
        return false;
    }

    return true;
};

//bool StringUtils::is_valid_float(const String &str) {

//    int len = str.length();

//    if (len == 0)
//        return false;

//    int from = 0;
//    if (str.front() == '+' || str.front() == '-') {
//        from++;
//    }

//    bool exponent_found = false;
//    bool period_found = false;
//    bool sign_found = false;
//    bool exponent_values_found = false;
//    bool numbers_found = false;

//    for (int i = from; i < len; i++) {

//        if (str[i].isDigit()) {

//            if (exponent_found)
//                exponent_values_found = true;
//            else
//                numbers_found = true;
//        } else if (numbers_found && !exponent_found && str[i] == 'e') {
//            exponent_found = true;
//        } else if (!period_found && !exponent_found && str[i] == '.') {
//            period_found = true;
//        } else if ((str[i] == '-' || str[i] == '+') && exponent_found && !exponent_values_found && !sign_found) {
//            sign_found = true;
//        } else
//            return false; // no start with number plz
//    }

//    return numbers_found;
//}
bool StringUtils::is_valid_float(se_string_view str) {

    size_t len = str.length();

    if (len == 0)
        return false;

    size_t from = 0;
    if (str.front() == '+' || str.front() == '-') {
        from++;
    }

    bool exponent_found = false;
    bool period_found = false;
    bool sign_found = false;
    bool exponent_values_found = false;
    bool numbers_found = false;

    for (size_t i = from; i < len; i++) {

        if (isdigit(str[i])) {

            if (exponent_found)
                exponent_values_found = true;
            else
                numbers_found = true;
        } else if (numbers_found && !exponent_found && str[i] == 'e') {
            exponent_found = true;
        } else if (!period_found && !exponent_found && str[i] == '.') {
            period_found = true;
        } else if ((str[i] == '-' || str[i] == '+') && exponent_found && !exponent_values_found && !sign_found) {
            sign_found = true;
        } else
            return false; // no start with number plz
    }

    return numbers_found;
}
String PathUtils::path_to_file(se_string_view base,se_string_view p_path) {

    // Don't get base dir for src, this is expected to be a dir already.
    String src = from_native_path(base);
    String dst = get_base_dir(from_native_path(p_path));
    String rel = path_to(src,dst);
    if (rel == dst) // failed
        return String(p_path);

    return String(rel) + get_file(p_path);
}
//String PathUtils::path_to(const String &str,String p_path) {

//    QString src = PathUtils::from_native_path(str);
//    QString dst = PathUtils::from_native_path(p_path);
//    if (!src.endsWith("/"))
//        src += "/";
//    if (!dst.endsWith("/"))
//        dst += "/";

//    String base;

//    if (src.startsWith("res://") && dst.startsWith("res://")) {

//        base = "res:/";
//        src.replace("res://", "/");
//        dst.replace("res://", "/");

//    } else if (src.startsWith("user://") && dst.startsWith("user://")) {

//        base = "user:/";
//        src.replace("user://", "/");
//        dst.replace("user://", "/");

//    } else if (src.startsWith("/") && dst.startsWith("/")) {

//        //nothing
//    } else {
//        //dos style
//        String src_begin = StringUtils::get_slice(src,'/', 0);
//        String dst_begin = StringUtils::get_slice(dst,'/', 0);

//        if (src_begin != dst_begin)
//            return p_path; //impossible to do this

//        base = src_begin;
//        src = src.mid(src_begin.length(), src.length());
//        dst = dst.mid(dst_begin.length(), dst.length());
//    }

//    //remove leading and trailing slash and split
//    auto src_dirs = src.mid(1, src.length() - 2).splitRef("/");
//    auto dst_dirs = dst.mid(1, dst.length() - 2).splitRef("/");

//    //find common parent
//    int common_parent = 0;

//    while (true) {
//        if (src_dirs.size() == common_parent)
//            break;
//        if (dst_dirs.size() == common_parent)
//            break;
//        if (src_dirs[common_parent] != dst_dirs[common_parent])
//            break;
//        common_parent++;
//    }

//    common_parent--;

//    QString dir;

//    for (int i = src_dirs.size() - 1; i > common_parent; i--) {

//        dir += "../";
//    }

//    for (int i = common_parent + 1; i < dst_dirs.size(); i++) {

//        dir += dst_dirs[i] + "/";
//    }

//    if (dir.length() == 0)
//        dir = "./";
//    return String(dir);
//}
String PathUtils::path_to(se_string_view str,se_string_view p_path) {

    String src = from_native_path(str);
    String dst = from_native_path(p_path);
    if (!src.ends_with("/"))
        src += "/";
    if (!dst.ends_with("/"))
        dst += "/";

    String base;

    if (src.starts_with("res://") && dst.starts_with("res://")) {

        base = "res:/";
        src.replace("res://", "/");
        dst.replace("res://", "/");

    } else if (src.starts_with("user://") && dst.starts_with("user://")) {

        base = "user:/";
        src.replace("user://", "/");
        dst.replace("user://", "/");

    } else if (src.starts_with("/") && dst.starts_with("/")) {

        //nothing
    } else {
        //dos style
        se_string_view src_begin(StringUtils::get_slice(src,'/', 0));
        se_string_view dst_begin(StringUtils::get_slice(dst,'/', 0));

        if (src_begin != dst_begin)
            return String(p_path); //impossible to do this

        base = src_begin;
        src = src.substr(src_begin.length(), src.length());
        dst = dst.substr(dst_begin.length(), dst.length());
    }

    //remove leading and trailing slash and split
    auto src_dirs = StringUtils::split(src.substr(1, src.length() - 2),"/");
    auto dst_dirs = StringUtils::split(dst.substr(1, dst.length() - 2),"/");

    //find common parent
    size_t common_parent = 0;

    while (true) {
        if (src_dirs.size() == common_parent)
            break;
        if (dst_dirs.size() == common_parent)
            break;
        if (src_dirs[common_parent] != dst_dirs[common_parent])
            break;
        common_parent++;
    }

    common_parent--;

    String dir;

    for (int i = src_dirs.size() - 1; i > common_parent; i--) {

        dir += "../";
    }

    for (size_t i = common_parent + 1; i < dst_dirs.size(); i++) {

        dir += String(dst_dirs[i]) + "/";
    }

    if (dir.length() == 0)
        dir = "./";
    return dir;
}

bool StringUtils::is_valid_filename(const UIString &str) {

    UIString stripped =strip_edges( str);
    if (str != stripped) {
        return false;
    }

    if (stripped.isEmpty()) {
        return false;
    }
    //TODO: SEGS: convert this chain of string scans to something saner.
    return !(str.contains(":") || str.contains('/') || str.contains('\\') || str.contains('?') ||
             str.contains('*') || str.contains('\"') || str.contains('|') || str.contains('%') || str.contains('<') ||
             str.contains('>'));
}
bool StringUtils::is_valid_filename(se_string_view str) {

    se_string_view stripped = strip_edges( str);
    if (str != stripped) {
        return false;
    }

    if (stripped.empty()) {
        return false;
    }
    return str.find_first_of(":/\\?*\"|%<>")==String::npos;
}
bool StringUtils::is_valid_ip_address(se_string_view str) {

    if (contains(str,':')) {
        FixedVector<se_string_view,8,true> ip;
        String::split_ref(ip,str,':');
        for (size_t i = 0; i < ip.size(); i++) {

            se_string_view n = ip[i];
            if (n.empty())
                continue;
            if (is_valid_hex_number(n,false)) {
                int nint = hex_to_int(n,false);
                if (nint < 0 || nint > 0xffff)
                    return false;
                continue;
            }
            if (!is_valid_ip_address(n))
                return false;
        }

    } else {
        FixedVector<se_string_view,4,false> ip;
        String::split_ref(ip,str,'.');
        if (ip.size() != 4)
            return false;
        for (size_t i = 0; i < ip.size(); i++) {

            se_string_view n = ip[i];
            if (!is_valid_integer(n))
                return false;
            int val = to_int(n);
            if (val < 0 || val > 255)
                return false;
        }
    }

    return true;
}

bool PathUtils::is_resource_file(const UIString &str) {

    return StringUtils::begins_with(str,"res://") && !str.contains("::");
}
bool PathUtils::is_resource_file(se_string_view str) {

    return StringUtils::begins_with(str,"res://") && !str.contains("::");
}
bool PathUtils::is_rel_path(const UIString &str) {

    return !is_abs_path(str);
}
bool PathUtils::is_rel_path(se_string_view str) {

    return !is_abs_path(str);
}

se_string_view PathUtils::trim_trailing_slash(se_string_view path) {
    CharType last_char = path.back();
    if(last_char=='/' || last_char=='\\')
        return StringUtils::substr(path,0,path.size()-1);
    return path;
}
String PathUtils::get_base_dir(se_string_view path) {

    auto basepos = StringUtils::find(path,"://");
    se_string_view rs;
    se_string_view base;
    if (basepos != String::npos) {
        int end = basepos + 3;
        rs = StringUtils::substr(path,end);
        base = StringUtils::substr(path,0, end);
    } else {
        if (path.starts_with('/')) {
            rs = StringUtils::substr(path,1);
            base = "/";
        } else {

            rs = path;
        }
    }

    auto parent_path = PathUtils::path(rs);
    if(parent_path==se_string_view("."))
        return String(base);
    return String(base) + parent_path;
}
se_string_view PathUtils::get_file(se_string_view path) {
    auto pos = path.find_last_of("/\\");
    if (pos == String::npos)
        return path;
    return path.substr(pos + 1);
}
UIString PathUtils::get_extension(const UIString &path) {

    int pos = path.lastIndexOf(".");
    if (pos < 0 || pos < MAX(path.lastIndexOf("/"), path.lastIndexOf("\\")))
        return UIString();

    return StringUtils::substr(path,pos + 1);
}
se_string_view PathUtils::get_extension(se_string_view path) {
    auto pos = path.rfind(".");
    if(pos == String::npos)
        return se_string_view();
    auto sep = path.find_last_of("/\\");
    if(sep!=String::npos && pos<sep)
        return se_string_view();

    return StringUtils::substr(path,pos + 1);
}
//String PathUtils::plus_file(const String &bp,const String &p_file) {
//    if (bp.isEmpty())
//        return p_file;
//    if (bp.back() == '/' || StringUtils::begins_with(p_file,'/'))
//        return bp + p_file;
//    return bp + "/" + p_file;
//}
//String PathUtils::plus_file_utf8(const String &bp,se_string_view p_file) {
//    String fl(String::fromUtf8(p_file.data(),p_file.size()));
//    if (bp.isEmpty())
//        return fl;
//    if (bp.back() == '/' || StringUtils::begins_with(p_file,"/"))
//        return bp + fl;
//    return bp + "/" + fl;
//}
//String PathUtils::plus_file_utf8(se_string_view bp,se_string_view p_file) {
//    if (bp.empty())
//        return String(p_file);
//    if (bp.back() == '/' || StringUtils::begins_with(p_file,"/"))
//        return String(bp) + p_file;
//    return String(bp) + "/" + p_file;
//}
String PathUtils::plus_file(se_string_view bp,se_string_view p_file) {
    if (bp.empty())
        return String(p_file);
    if (p_file.empty())
        return String(bp)+"/";
    if (bp.back() == '/' || p_file.front()=='/')
        return String(bp) + p_file;
    return String(bp) + "/" + p_file;
}

UIString StringUtils::percent_encode(const UIString &str) {

    String cs = to_utf8(str);
    UIString encoded;
    for (size_t i = 0; i < cs.length(); i++) {
        char c = cs[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '~' || c == '.') {
            encoded += c;
        } else {
            char p[4] = { '%', 0, 0, 0 };
            static const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

            p[1] = hex[c >> 4];
            p[2] = hex[c & 0xF];
            encoded.append(p);
        }
    }

    return encoded;
}
String StringUtils::percent_encode(se_string_view cs) {

    String encoded;
    for (size_t i = 0; i < cs.length(); i++) {
        char c = cs[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '~' || c == '.') {
            encoded += c;
        } else {
            char p[4] = { '%', 0, 0, 0 };
            static const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

            p[1] = hex[c >> 4];
            p[2] = hex[c & 0xF];
            encoded.append(p);
        }
    }

    return encoded;
}
UIString StringUtils::percent_decode(const UIString &str) {
    String cs = to_utf8(str);
    String s=percent_decode(se_string_view(cs.data(),cs.size()));
    return QString::fromUtf8(s.data(),s.size());
}
String StringUtils::percent_decode(se_string_view str) {

    String pe;

    for (size_t i = 0; i < str.length(); i++) {

        char c = str[i];
        if (c == '%' && i < str.length() - 2) {

            char a = tolower(str[i + 1]);
            char b = tolower(str[i + 2]);

            if (a >= '0' && a <= '9')
                c = char((a - '0') << 4);
            else if (a >= 'a' && a <= 'f')
                c = char((a - 'a' + 10) << 4);
            else
                continue;

            uint8_t d = 0;

            if (b >= '0' && b <= '9')
                d = (b - '0');
            else if (b >= 'a' && b <= 'f')
                d = (b - 'a' + 10);
            else
                continue;
            c += d;
            i += 2;
        }
        pe += c;
    }
    return pe;
}

UIString PathUtils::get_basename(const UIString &path) {

    int pos = path.lastIndexOf('.');
    if (pos < 0 || pos < MAX(path.lastIndexOf("/"), path.lastIndexOf("\\")))
        return path;

    return StringUtils::substr(path,0, pos);
}
se_string_view PathUtils::get_basename(se_string_view path) {

    auto pos = path.rfind('.');
    if(pos==String::npos)
        return path;
    auto file = get_file(path);
    return file.substr(0,file.rfind('.'));
}
se_string_view PathUtils::path(se_string_view path) {

    auto last_slash_pos = path.find_last_of("/\\");
    if(last_slash_pos==String::npos)
        return ".";
    return path.substr(0,last_slash_pos);
}
String itos(int64_t p_val) {

    return StringUtils::num_int64(p_val);
}

String rtos(double p_val) {

    return StringUtils::num(p_val);
}

String rtoss(double p_val) {

    return StringUtils::num_scientific(p_val);
}

// Right-pad with a character.
UIString StringUtils::rpad(const UIString &src,int min_length, char character)  {
    UIString s = src;
    int padding = min_length - s.length();
    if (padding > 0) {
        for (int i = 0; i < padding; i++)
            s = s + character;
    }

    return s;
}
String StringUtils::rpad(const String &src,int min_length, char character)  {
    String s = src;
    int padding = min_length - s.length();
    if (padding > 0) {
        for (int i = 0; i < padding; i++)
            s = s + character;
    }

    return s;
}
// Left-pad with a character.
UIString StringUtils::lpad(const UIString &src,int min_length, char character)  {
    UIString s = src;
    int padding = min_length - s.length();
    if (padding > 0) {
        for (int i = 0; i < padding; i++)
            s = character + s;
    }

    return s;
}
String StringUtils::lpad(const String &src,int min_length, char character)  {
    String s = src;
    int padding = min_length - s.length();
    if (padding > 0) {
        for (int i = 0; i < padding; i++)
            s = character + s;
    }

    return s;
}

// sprintf is implemented in GDScript via:
//   "fish %s pie" % "frog"
//   "fish %s %d pie" % ["frog", 12]
// In case of an error, the string returned is the error description and "error" is true.
String StringUtils::sprintf(se_string_view str,const Array &values, bool *error) {
    String formatted;
    const char *self = str.data();
    bool in_format = false;
    int value_index = 0;
    int min_chars = 0;
    int min_decimals = 0;
    bool in_decimals = false;
    bool pad_with_zeroes = false;
    bool left_justified = false;
    bool show_sign = false;

    *error = true;

    for (size_t idx=0; idx<str.length(); self++,++idx) {
        const char c = *self;

        if (in_format) { // We have % - lets see what else we get.
            switch (c) {
                case '%': { // Replace %% with %
                    formatted += c;
                    in_format = false;
                    break;
                }
                case 'd': // Integer (signed)
                case 'o': // Octal
                case 'x': // Hexadecimal (lowercase)
                case 'X': { // Hexadecimal (uppercase)
                    if (value_index >= values.size()) {
                        return ("not enough arguments for format string");
                    }

                    if (!values[value_index].is_num()) {
                        return ("a number is required");
                    }

                    int64_t value = values[value_index];
                    int base = 16;
                    bool capitalize = false;
                    switch (c) {
                        case 'd': base = 10; break;
                        case 'o': base = 8; break;
                        case 'x': break;
                        case 'X':
                            base = 16;
                            capitalize = true;
                            break;
                    }
                    // Get basic number.
                    String str = num_int64(ABS(value), base, capitalize);
                    int number_len = str.length();

                    // Padding.
                    char pad_char = pad_with_zeroes ? '0' : ' ';
                    if (left_justified) {
                        str = rpad(str,min_chars, pad_char);
                    } else {
                        str = lpad(str,min_chars, pad_char);
                    }

                    // Sign.
                    if (show_sign && value >= 0) {
                        str = str.insert(pad_with_zeroes ? 0 : str.length() - number_len, "+");
                    } else if (value < 0) {
                        str = str.insert(pad_with_zeroes ? 0 : str.length() - number_len, "-");
                    }

                    formatted += str;
                    ++value_index;
                    in_format = false;

                    break;
                }
                case 'f': { // Float
                    if (value_index >= values.size()) {
                        return ("not enough arguments for format string");
                    }

                    if (!values[value_index].is_num()) {
                        return ("a number is required");
                    }

                    double value = values[value_index].as<float>();
                    String str = num(value, min_decimals);

                    // Pad decimals out.
                    str = pad_decimals(str,min_decimals);

                    // Show sign
                    if (show_sign && left(str,1) != "-"_sv) {
                        str = str.insert(0, "+");
                    }

                    // Padding
                    if (left_justified) {
                        str = rpad(str,min_chars);
                    } else {
                        str = lpad(str,min_chars);
                    }

                    formatted += str;
                    ++value_index;
                    in_format = false;

                    break;
                }
                case 's': { // String
                    if (value_index >= values.size()) {
                        return ("not enough arguments for format string");
                    }

                    String str = values[value_index].as<String>();
                    // Padding.
                    if (left_justified) {
                        str = rpad(str,min_chars);
                    } else {
                        str = lpad(str,min_chars);
                    }

                    formatted += str;
                    ++value_index;
                    in_format = false;
                    break;
                }
                case 'c': {
                    if (value_index >= values.size()) {
                        return ("not enough arguments for format string");
                    }

                    // Convert to character.
                    String str;
                    if (values[value_index].is_num()) {
                        int value = values[value_index];
                        if (value < 0) {
                            return ("unsigned byte integer is lower than maximum");
                        } else if (value > 255) {
                            return ("unsigned byte integer is greater than maximum");
                        }
                        str.push_back(values[value_index].as<int>());
                    } else if (values[value_index].get_type() == VariantType::STRING) {
                        str = values[value_index].as<String>();
                        if (str.length() != 1) {
                            return ("%c requires number or single-character string");
                        }
                    } else {
                        return ("%c requires number or single-character string");
                    }

                    // Padding.
                    if (left_justified) {
                        str = rpad(str,min_chars);
                    } else {
                        str = lpad(str,min_chars);
                    }

                    formatted += str;
                    ++value_index;
                    in_format = false;
                    break;
                }
                case '-': { // Left justify
                    left_justified = true;
                    break;
                }
                case '+': { // Show + if positive.
                    show_sign = true;
                    break;
                }
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': {
                    int n = c-'0';
                    if (in_decimals) {
                        min_decimals *= 10;
                        min_decimals += n;
                    } else {
                        if (c == '0' && min_chars == 0) {
                            pad_with_zeroes = true;
                        } else {
                            min_chars *= 10;
                            min_chars += n;
                        }
                    }
                    break;
                }
                case '.': { // Float separator.
                    if (in_decimals) {
                        return ("too many decimal points in format");
                    }
                    in_decimals = true;
                    min_decimals = 0; // We want to add the value manually.
                    break;
                }

                case '*': { // Dynamic width, based on value.
                    if (value_index >= values.size()) {
                        return ("not enough arguments for format string");
                    }

                    if (!values[value_index].is_num()) {
                        return ("* wants number");
                    }

                    int size = values[value_index];

                    if (in_decimals) {
                        min_decimals = size;
                    } else {
                        min_chars = size;
                    }

                    ++value_index;
                    break;
                }

                default: {
                    return ("unsupported format character");
                }
            }
        } else { // Not in format string.
            switch (c) {
                case '%':
                    in_format = true;
                    // Back to defaults:
                    min_chars = 0;
                    min_decimals = 6;
                    pad_with_zeroes = false;
                    left_justified = false;
                    show_sign = false;
                    in_decimals = false;
                    break;
                default:
                    formatted += c;
            }
        }
    }

    if (in_format) {
        return ("incomplete format");
    }

    if (value_index != values.size()) {
        return ("not all arguments converted during string formatting");
    }

    *error = false;
    return formatted;
}

UIString StringUtils::quote(const UIString &str,char character) {
    return character + str + character;
}
String StringUtils::quote(se_string_view str,char character) {
    return character+String(str)+character;
}
UIString StringUtils::unquote(const UIString &str) {
    if (!is_quoted(str)) {
        return str;
    }

    return substr(str,1, str.length() - 2);
}
se_string_view StringUtils::unquote(se_string_view str) {
    if (!is_quoted(str)) {
        return str;
    }

    return str.substr(1, str.length() - 2);
}
#ifdef TOOLS_ENABLED
StringName TTR(se_string_view p_text) {

    if (TranslationServer::get_singleton()) {
        return TranslationServer::get_singleton()->tool_translate(StringName(p_text));
    }

    return StringName(p_text);
}

#endif

StringName RTR(const char *p_text) {

    if (TranslationServer::get_singleton()) {
        StringName rtr(TranslationServer::get_singleton()->tool_translate(StringName(p_text)));
        if (rtr.empty() || rtr == p_text) {
            return TranslationServer::get_singleton()->translate(StringName(p_text));
        } else {
            return rtr;
        }
    }

    return StringName(p_text);
}
String RTR_utf8(se_string_view sv) {
    return String(RTR(String(sv).c_str())).data();
}

int StringUtils::compare(const UIString &lhs, const UIString &rhs, Compare case_sensitive)
{
    if(case_sensitive==CaseSensitive)
    {
        return lhs.compare(rhs,Qt::CaseSensitive);
    } else if(case_sensitive==CaseInsensitive)
    {
        return lhs.compare(rhs,Qt::CaseInsensitive);
    }
    QCollator col;
    col.setNumericMode(true);
    return col.compare(lhs,rhs);
}
int StringUtils::compare(se_string_view lhs, se_string_view rhs, Compare case_sensitive)
{
    return compare(from_utf8(lhs),from_utf8(rhs),case_sensitive);
}
bool StringUtils::contains(const char *heystack, const char *needle)
{
    std::string_view sv1(heystack);
    std::string_view nd1(needle);
    return sv1.find(nd1)!=std::string_view::npos;
}
bool StringUtils::contains(const UIString &heystack, const UIString &needle,Compare mode)
{
    assert(mode!=Compare::CaseNatural);
    return heystack.contains(needle,mode==CaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
}
bool StringUtils::contains(const UIString &heystack, se_string_view needle,Compare mode)
{
    assert(mode!=Compare::CaseNatural);
    return heystack.contains(needle.data(),mode==CaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
}
bool StringUtils::contains(const UIString &heystack, CharType c, Compare)
{
    return heystack.contains(c);
}
bool StringUtils::contains(se_string_view heystack, char c)
{
    return heystack.find(c)!=heystack.npos;
}
bool StringUtils::contains(se_string_view heystack, se_string_view c)
{
    return heystack.find(c)!=heystack.npos;
}

bool PathUtils::is_internal_path(const UIString &path)
{
    return StringUtils::contains(path,UIString("local://")) || StringUtils::contains(path,UIString("::"));
}
bool PathUtils::is_internal_path(se_string_view path)
{
    return StringUtils::contains(path,("local://")) || StringUtils::contains(path,("::"));
}

UIString StringUtils::from_utf8(const char *p_utf8, int p_len)
{
    UIString res(QString::fromUtf8(p_utf8,p_len));
    return res;
}
UIString StringUtils::from_utf8(se_string_view p_utf8)
{
    UIString res(QString::fromUtf8(p_utf8.data(),p_utf8.length()));
    return res;
}

UIString StringUtils::from_wchar(const wchar_t *p_utf8, int p_len)
{
    return UIString { QString::fromWCharArray(p_utf8,p_len)};
}

void StringUtils::Inplace::replace(UIString &str, const char *p_key, const char *p_with)
{
    str.replace(p_key,p_with);
}

void StringUtils::Inplace::replace(UIString &str, CharType p_key, CharType p_with)
{
    str.replace(p_key,p_with);
}

void StringUtils::Inplace::replace(UIString &str, const UIString &p_key, const UIString &p_with)
{
    str.replace(p_key,p_with);
}
int StringUtils::char_length(const UIString &str) {
    return str.size();
}
String StringUtils::property_name_encode(se_string_view str) {
    // Escape and quote strings with extended ASCII or further Unicode characters
    // as well as '"', '=' or ' ' (32)
    for (char c : str) {
        if (c == '=' || c == '"' || c < 33 || c > 126) {
            return "\"" + c_escape_multiline(str) + "\"";
        }
    }
    // Keep as is
    return String(str);
}
namespace PathUtils {
UIString from_native_path(const UIString &p) {
    return UIString(p).replace('\\', '/');
}
String from_native_path(se_string_view p) {
    return StringUtils::replace(p,'\\', '/');
}

UIString to_win_path(const UIString &v)
{
    return UIString(v).replace("/", "\\");
}
String to_win_path(se_string_view v)
{
    return StringUtils::replace(v,"/", "\\");
}
}
