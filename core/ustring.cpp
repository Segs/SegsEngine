/*************************************************************************/
/*  ustring.cpp                                                          */
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

#include "ustring.h"

#include "core/color.h"
#include "core/crypto/crypto_core.h"
#include "core/math/math_funcs.h"
#include "core/os/memory.h"
#include "core/translation.h"
#include "core/list.h"
#include "core/vector.h"
#include "core/variant.h"

#include <QString>
#include <QVector>
#include <QCollator>

#include <cstdio>
#include <cstdlib>

//template class GODOT_EXPORT_TEMPLATE_B eastl::vector<String,wrap_allocator>;
/*
    TODO: SEGS: When replacing QString as the underlying string type consider the following helper class from qt
    QTextBoundaryFinder for grapheme navigation in QChar-like *strings
    QUtf8 from QtCore/private/qutfcodec_p.h>
    TODO: SEGS: consider splitting strings into two classes user visible and internal ?
*/

#if defined(MINGW_ENABLED) || defined(_MSC_VER)
#define snprintf _snprintf_s
#endif

#define MAX_DIGITS 6
namespace
{
bool is_enclosed_in(const String &str,const CharType p_char) {

    return StringUtils::begins_with(str,p_char) && StringUtils::ends_with(str,p_char);
}

}
const String String::null_val {};
const CharType String::_null = 0;

bool is_symbol(CharType c) {
    return c != '_' && ((c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') || (c >= '{' && c <= '~') || c == '\t' || c == ' ');
}

bool select_word(const String &p_s, int p_col, int &r_beg, int &r_end) {

    const String &s = p_s;
    int beg = CLAMP(p_col, 0, s.length());
    int end = beg;

    if (s[beg] > 32 || beg == s.length()) {

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
void String::copy_from_unchecked(const CharType *p_char, const int p_length) {
    m_str.resize(p_length);

    CharType *dst = data();

    for (int i = 0; i < p_length; i++) {
        dst[i] = p_char[i];
    }
}

bool String::operator==(const StrRange &p_str_range) const {

    int len = p_str_range.len;

    if (length() != len)
        return false;
    if (empty())
        return true;

    const CharType *c_str = p_str_range.c_str;
    const CharType *dst = cdata();

    /* Compare char by char */
    for (int i = 0; i < len; i++) {

        if (c_str[i] != dst[i])
            return false;
    }

    return true;
}

void StringUtils::erase(String &str,int p_pos, int p_chars) {

    str = StringUtils::left(str,p_pos) + StringUtils::substr(str,p_pos + p_chars);
}

String StringUtils::capitalize(const String &s) {

    String aux = StringUtils::strip_edges(StringUtils::replace(camelcase_to_underscore(s,true),"_", " "));
    String cap;
    for (int i = 0; i < StringUtils::get_slice_count(aux,' '); i++) {

        String slice = StringUtils::get_slice(aux,' ', i);
        if (!slice.empty()) {

            slice.set(0,StringUtils::char_uppercase(slice[0]));
            if (i > 0)
                cap += ' ';
            cap += slice;
        }
    }

    return cap;
}

String StringUtils::camelcase_to_underscore(const String &s,bool lowercase) {
    const CharType *cstr = s.cdata();
    String new_string;
    const char A = 'A', Z = 'Z';
    const char a = 'a', z = 'z';
    int start_index = 0;

    for (int i = 1; i < s.m_str.size(); i++) {
        bool is_upper = cstr[i] >= A && cstr[i] <= Z;
        bool is_number = cstr[i] >= '0' && cstr[i] <= '9';
        bool are_next_2_lower = false;
        bool is_next_lower = false;
        bool is_next_number = false;
        bool was_precedent_upper = cstr[i - 1] >= A && cstr[i - 1] <= Z;
        bool was_precedent_number = cstr[i - 1] >= '0' && cstr[i - 1] <= '9';

        if (i + 2 < s.m_str.size()) {
            are_next_2_lower = cstr[i + 1] >= a && cstr[i + 1] <= z && cstr[i + 2] >= a && cstr[i + 2] <= z;
        }

        if (i + 1 < s.m_str.size()) {
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
            new_string += StringUtils::substr(s,start_index, i - start_index) + "_";
            start_index = i;
        }
    }

    new_string += StringUtils::substr(s,start_index);
    return lowercase ? StringUtils::to_lower(new_string) : new_string;
}

int StringUtils::get_slice_count(const String &str,const String &p_splitter) {

    auto parts = str.m_str.splitRef(p_splitter.m_str);
    return parts.size();
}
int StringUtils::get_slice_count(const String &str,CharType p_splitter) {

    auto parts = str.m_str.splitRef(p_splitter);
    return parts.size();
}
String StringUtils::get_slice(const String &str,const String &p_splitter, int p_slice) {

    if (p_slice<0 || str.empty() || p_splitter.empty())
        return String();

    int pos = 0;
    int prev_pos = 0;

    if (StringUtils::find(str,p_splitter) == -1)
        return str;

    int i = 0;
    while (true) {

        pos = StringUtils::find(str,p_splitter, pos);
        if (pos == -1)
            pos = str.length(); //reached end

        int from = prev_pos;
        //int to=pos;

        if (p_slice == i) {

            return StringUtils::substr(str,from, pos - from);
        }

        if (pos == str.length()) //reached end and no find
            break;
        pos += p_splitter.length();
        prev_pos = pos;
        i++;
    }

    return String(); //no find!
}

String StringUtils::get_slice(const String &str,CharType p_splitter, int p_slice) {

    if (str.empty())
        return String();

    if (p_slice < 0)
        return String();

    const CharType *c = str.cdata();
    int i = 0;
    int fin=str.size();
    int prev = 0;
    int count = 0;
    for(i=0; i<fin; ++i) {

        if (c[i] == p_splitter) {
            if (p_slice == count) {
                return StringUtils::substr(str,prev, i - prev);
            } else {
                count++;
                prev = i + 1;
            }
        }
    }
    if (p_slice == count)
       return StringUtils::substr(str,prev);
    return String();
}

Vector<String> StringUtils::split_spaces(const String &str) {

    Vector<String> ret;
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

            ret.push_back(StringUtils::substr(str,from, i - from));
            inside = false;
        }
    }
    ret.push_back(StringUtils::substr(str,from));

    return ret;
}

Vector<String> StringUtils::split(const String &str,const String &p_splitter, bool p_allow_empty, int p_maxsplit) {

    Vector<String> ret;
    int from = 0;
    int len = str.length();

    while (true) {

        int end = StringUtils::find(str,p_splitter, from);
        if (end < 0)
            end = len;
        if (p_allow_empty || (end > from)) {
            if (p_maxsplit <= 0)
                ret.push_back(StringUtils::substr(str,from, end - from));
            else {

                // Put rest of the string and leave cycle.
                if (p_maxsplit == ret.size()) {
                    ret.push_back(StringUtils::substr(str,from));
                    break;
                }

                // Otherwise, push items until positive limit is reached.
                ret.push_back(StringUtils::substr(str,from, end - from));
            }
        }

        if (end == len)
            break;

        from = end + p_splitter.length();
    }

    return ret;
}
Vector<String> StringUtils::split(const String &str,const CharType p_splitter, bool p_allow_empty) {
    Vector<String> ret;
    auto val = str.m_str.splitRef(p_splitter,p_allow_empty ? QString::KeepEmptyParts : QString::SkipEmptyParts);
    ret.resize(val.size());
    for(int i=0,fin=val.size(); i<fin; ++i)
        ret.write[i] = val[i].toString();
    return ret;
}
Vector<String> StringUtils::rsplit(const String &str,const String &p_splitter, bool p_allow_empty, int p_maxsplit) {

    Vector<String> ret;
    const int len = str.length();
    int remaining_len = len;

    while (true) {

        if (remaining_len < p_splitter.length() || (p_maxsplit > 0 && p_maxsplit == ret.size())) {
            // no room for another splitter or hit max splits, push what's left and we're done
            if (p_allow_empty || remaining_len > 0) {
                ret.push_back(StringUtils::substr(str,0, remaining_len));
            }
            break;
        }

        int left_edge = StringUtils::rfind(str,p_splitter, remaining_len - p_splitter.length());

        if (left_edge < 0) {
            // no more splitters, we're done
            ret.push_back(StringUtils::substr(str,0, remaining_len));
            break;
        }

        int substr_start = left_edge + p_splitter.length();
        if (p_allow_empty || substr_start < remaining_len) {
            ret.push_back(StringUtils::substr(str,substr_start, remaining_len - substr_start));
        }

        remaining_len = left_edge;
    }

    ret.invert();
    return ret;
}

Vector<float> StringUtils::split_floats(const String &str,const String &p_splitter, bool p_allow_empty) {

    Vector<float> ret;
    int from = 0;
    int len = str.length();

    while (true) {

        int end = StringUtils::find(str,p_splitter, from);
        if (end < 0)
            end = len;
        if (p_allow_empty || (end > from))
            ret.push_back(StringUtils::to_double(str.cdata()+from));

        if (end == len)
            break;

        from = end + p_splitter.length();
    }

    return ret;
}

Vector<float> StringUtils::split_floats_mk(const String &str,const Vector<String> &p_splitters, bool p_allow_empty) {

    Vector<float> ret;
    int from = 0;
    int len = str.length();

    while (true) {

        int idx;
        int end = StringUtils::findmk(str,p_splitters, from, &idx);
        int spl_len = 1;
        if (end < 0) {
            end = len;
        } else {
            spl_len = p_splitters[idx].length();
        }

        if (p_allow_empty || (end > from)) {
            ret.push_back(StringUtils::to_double(str.cdata()+from));
        }

        if (end == len)
            break;

        from = end + spl_len;
    }

    return ret;
}

Vector<int> StringUtils::split_ints(const String &str,const String &p_splitter, bool p_allow_empty) {

    Vector<int> ret;
    int from = 0;
    int len = str.length();

    while (true) {

        int end = StringUtils::find(str,p_splitter, from);
        if (end < 0)
            end = len;
        if (p_allow_empty || (end > from))
            ret.push_back(StringUtils::to_int(str.cdata()+from, end - from));

        if (end == len)
            break;

        from = end + p_splitter.length();
    }

    return ret;
}

Vector<int> StringUtils::split_ints_mk(const String &str,const Vector<String> &p_splitters, bool p_allow_empty) {

    Vector<int> ret;
    int from = 0;
    int len = str.length();

    while (true) {

        int idx;
        int end = StringUtils::findmk(str,p_splitters, from, &idx);
        int spl_len = 1;
        if (end < 0) {
            end = len;
        } else {
            spl_len = p_splitters[idx].length();
        }

        if (p_allow_empty || (end > from))
            ret.push_back(StringUtils::to_int(str.cdata()+from, end - from));

        if (end == len)
            break;

        from = end + spl_len;
    }

    return ret;
}

String StringUtils::join(const String &str,const Vector<String> &parts) {
    String ret;
    for (int i = 0; i < parts.size(); ++i) {
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

String StringUtils::to_upper(const String &str){

    return str.m_str.toUpper();
}

String StringUtils::to_lower(const String &str) {
    return str.m_str.toLower();
}

String StringUtils::md5(const uint8_t *p_md5) {
    return StringUtils::hex_encode_buffer(p_md5, 16);
}

String StringUtils::hex_encode_buffer(const uint8_t *p_buffer, int p_len) {
    static const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

    String ret;
    ret.m_str.reserve(p_len*2);

    char v[2] = { 0, 0 };

    for (int i = 0; i < p_len; i++) {
        v[0] = hex[p_buffer[i] >> 4];
        ret.m_str += v;
        v[0] = hex[p_buffer[i] & 0xF];
        ret.m_str += v;
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

    return String(buf);
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
    s.m_str.resize(chars);
    CharType *c = s.data();

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
    s.m_str.resize(chars);
    CharType *c = s.data();
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

            CharType num = '0' + (intn % 10);
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
    return QString::number(p_num,'g');
}

CharString StringUtils::ascii(const String &str,bool p_allow_extended) {
    if(p_allow_extended)
        return str.m_str.toLocal8Bit();
    else
        return str.m_str.toLatin1();
}

bool StringUtils::parse_utf8(String &str,const char *p_utf8, int p_len) {

    if (!p_utf8)
        return true;

    str.m_str = QString::fromUtf8(p_utf8, p_len);
    return !str.empty();
}

CharString StringUtils::utf8(const String &str) {
    return str.m_str.toUtf8();
}

String::String(const StrRange &p_range) {

    if (!p_range.c_str)
        return;
    m_str.append(p_range.c_str, p_range.len);
}

int StringUtils::hex_to_int(const String &s,bool p_with_prefix) {

    if (p_with_prefix && s.length() < 3)
        return 0;
    QStringRef to_convert;
    if (p_with_prefix) {
        if (!StringUtils::begins_with(s,"0x"))
            return 0;
         to_convert = s.m_str.midRef(2);
    }
    else
        to_convert = s.m_str.midRef(0);
    return to_convert.toInt(nullptr,16);
}

int64_t StringUtils::hex_to_int64(const String &s,bool p_with_prefix) {

    if (p_with_prefix && s.length() < 3)
        return 0;
    QStringRef to_convert;
    if (p_with_prefix) {
        if (!s.m_str.startsWith("0x"))
            return 0;
         to_convert = s.m_str.midRef(2);
    }
    else
        to_convert = s.m_str.midRef(0);
    return to_convert.toLongLong(nullptr,16);
}
int64_t StringUtils::bin_to_int64(const String &s,bool p_with_prefix) {

    if (p_with_prefix && s.length() < 3)
        return 0;
    QStringRef to_convert;
    if (p_with_prefix) {
        if (!s.m_str.startsWith("0b"))
            return 0;
         to_convert = s.m_str.midRef(2);
    }
    else
        to_convert = s.m_str.midRef(0);
    return to_convert.toLongLong(nullptr,2);
}

int64_t StringUtils::to_int64(const String &s) {
    return s.m_str.toLongLong();
}

int StringUtils::to_int(const char *p_str, int p_len) {
    return QByteArray::fromRawData(p_str,p_len).toInt();

}

bool StringUtils::is_numeric(const String &str) {

    if (str.length() == 0) {
        return false;
    }

    int s = 0;
    if (str[0] == '-') ++s;
    bool dot = false;
    for (int i = s; i < str.length(); i++) {

        CharType c = str[i];
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

    int sign, expSign = false;
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

double StringUtils::to_double(const char *p_str) {
    return QByteArray::fromRawData(p_str,qstrlen(p_str)).toDouble();
}

float StringUtils::to_float(const String &s) {
    return s.m_str.toFloat();
}

double StringUtils::to_double(const CharType *p_str, const CharType **r_end) {
    return built_in_strtod(p_str, r_end);
}

int64_t StringUtils::to_int(const CharType *p_str, int p_len) {

    if (p_len == 0 || p_str[0].isNull())
        return 0;
    return QString::fromRawData(p_str,p_len).toLongLong();
}

double StringUtils::to_double(const String &s) {
    return s.m_str.toDouble();
}

String StringUtils::md5_text(const String &str) {

    CharString cs = StringUtils::to_utf8(str);
    unsigned char hash[16];
    CryptoCore::md5((unsigned char *)cs.data(), cs.length(), hash);
    return StringUtils::hex_encode_buffer(hash, 16);
}

String StringUtils::sha1_text(const String &str) {
    CharString cs = StringUtils::to_utf8(str);
    unsigned char hash[20];
    CryptoCore::sha1((unsigned char *)cs.data(), cs.length(), hash);
    return StringUtils::hex_encode_buffer(hash, 20);
}

String StringUtils::sha256_text(const String &str) {
    CharString cs = StringUtils::to_utf8(str);
    unsigned char hash[32];
    CryptoCore::sha256((unsigned char *)cs.data(), cs.length(), hash);
    return StringUtils::hex_encode_buffer(hash, 32);
}

Vector<uint8_t> StringUtils::md5_buffer(const String &str) {

    CharString cs = StringUtils::to_utf8(str);
    unsigned char hash[16];
    CryptoCore::md5((unsigned char *)cs.data(), cs.length(), hash);

    Vector<uint8_t> ret;
    ret.resize(16);
    for (int i = 0; i < 16; i++) {
        ret.write[i] = hash[i];
    }
    return ret;
};

Vector<uint8_t> StringUtils::sha1_buffer(const String &str) {
    CharString cs = StringUtils::to_utf8(str);
    unsigned char hash[20];
    CryptoCore::sha1((unsigned char *)cs.data(), cs.length(), hash);

    Vector<uint8_t> ret;
    ret.resize(20);
    for (int i = 0; i < 20; i++) {
        ret.write[i] = hash[i];
    }

    return ret;
}

Vector<uint8_t> StringUtils::sha256_buffer(const String &str)  {
    CharString cs = StringUtils::to_utf8(str);
    unsigned char hash[32];
    CryptoCore::sha256((unsigned char *)cs.data(), cs.length(), hash);

    Vector<uint8_t> ret;
    ret.resize(32);
    for (int i = 0; i < 32; i++) {
        ret.write[i] = hash[i];
    }
    return ret;
}

String StringUtils::insert(const String &s,int p_at_pos, const String &p_string) {
    String res(s);
    res.m_str.insert(p_at_pos,p_string.m_str);
    return res;
}
String StringUtils::substr(const String &s,int p_from, int p_chars) {

    if(s.empty())
        return s;
    if ((p_from + p_chars) > s.length()) {

        p_chars = s.length() - p_from;
    }
//    if ((p_from + p_chars) == s.length()) {
//        printf("Simplify me");
//    }
    return s.m_str.mid(p_from,p_chars);
}

int StringUtils::find_last(const String &s,const String &p_str) {
    return s.m_str.lastIndexOf(p_str.m_str);
}
int StringUtils::find_last(const String &s,const CharType c) {
    return s.m_str.lastIndexOf(c);
}

int StringUtils::find(const String &s,const String &p_str, int p_from) {
    if (p_from < 0)
        return -1;
    return s.m_str.indexOf(p_str.m_str,p_from);
}

int StringUtils::find(const String &s,const char *p_str, int p_from) {

    if (p_from < 0)
        return -1;
    return s.m_str.indexOf(QLatin1String(p_str),p_from);
}

int StringUtils::find_char(const String &s, CharType p_char, int p_from) {
    return s.m_str.indexOf(p_char, p_from);
}

int StringUtils::findmk(const String &s,const Vector<String> &p_keys, int p_from, int *r_key) {

    if (p_from < 0)
        return -1;
    if (p_keys.empty())
        return -1;

    //int src_len=p_str.length();
    const String *keys = &p_keys[0];
    int key_count = p_keys.size();
    int len = s.length();

    if (len == 0)
        return -1; // won't find anything!

    const CharType *src = s.cdata();

    for (int i = p_from; i < len; i++) {

        bool found = true;
        for (int k = 0; k < key_count; k++) {

            found = true;
            if (r_key)
                *r_key = k;
            const CharType *cmp = keys[k].cdata();
            int l = keys[k].length();

            for (int j = 0; j < l; j++) {

                int read_pos = i + j;

                if (read_pos >= len) {

                    found = false;
                    break;
                }

                if (src[read_pos] != cmp[j]) {
                    found = false;
                    break;
                }
            }
            if (found)
                break;
        }

        if (found)
            return i;
    }

    return -1;
}

int StringUtils::findn(const String &s,const String &p_str, int p_from) {

    if (p_from < 0)
        return -1;

    int src_len = p_str.length();

    if (src_len == 0 || s.length() == 0)
        return -1; // won't find anything!

    const CharType *srcd = s.cdata();

    for (int i = p_from; i <= (s.length() - src_len); i++) {

        bool found = true;
        for (int j = 0; j < src_len; j++) {

            int read_pos = i + j;

            if (read_pos >= s.length()) {

                ERR_PRINT("read_pos>=length()")
                return -1;
            }

            CharType src = srcd[read_pos].toLower();
            CharType dst = p_str[j].toLower();

            if (src != dst) {
                found = false;
                break;
            }
        }

        if (found)
            return i;
    }

    return -1;
}

int StringUtils::rfind(const String &s,const String &p_str, int p_from) {
    return s.m_str.lastIndexOf(p_str.m_str,p_from);
}
int StringUtils::rfindn(const String &s,const String &p_str, int p_from) {
    return s.m_str.lastIndexOf(p_str.m_str,p_from,Qt::CaseInsensitive);
}

bool StringUtils::ends_with(const String &s,const String &p_string) {
    return s.m_str.endsWith(p_string.m_str);
}
bool StringUtils::ends_with(const String &s,CharType p_char) {
    return s.m_str.endsWith(p_char);
}

bool StringUtils::begins_with(const String &str,CharType ch) {
    return str.m_str.startsWith(ch);
}
bool StringUtils::begins_with(const String &s,const String &p_string) {
    return s.m_str.startsWith(p_string.m_str);
}
bool StringUtils::begins_with(const String &s,const char *p_string) {
    return s.m_str.startsWith(p_string);
}

bool StringUtils::is_subsequence_of(const String &str,const String &p_string, Compare mode) {

    return p_string.m_str.startsWith(str.m_str,mode==CaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
}

bool StringUtils::is_quoted(const String &str) {

    return is_enclosed_in(str,'"') || is_enclosed_in(str,'\'');
}

static int str_count(const String &s,const String &p_string, int p_from, int p_to, bool p_case_insensitive)  {
    if (p_string.empty()) {
        return 0;
    }
    int len = s.length();
    int slen = p_string.length();
    if (len < slen) {
        return 0;
    }
    String str;
    if (p_from >= 0 && p_to >= 0) {
        if (p_to == 0) {
            p_to = len;
        } else if (p_from >= p_to) {
            return 0;
        }
        if (p_from == 0 && p_to == len) {
            str = s.m_str.mid(0,len);
        } else {
            str = s.m_str.mid(p_from, p_to - p_from);
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


int StringUtils::count(const String &s,const String &p_string, int p_from, int p_to) {
return str_count(s,p_string, p_from, p_to, false);
}

int StringUtils::countn(const String &s,const String &p_string, int p_from, int p_to) {

    return str_count(s,p_string, p_from, p_to, true);
}

Vector<String> StringUtils::bigrams(const String &str) {
    int n_pairs = str.length() - 1;
    Vector<String> b;
    if (n_pairs <= 0) {
        return b;
    }
    b.resize(n_pairs);
    for (int i = 0; i < n_pairs; i++) {
        b.write[i] = StringUtils::substr(str,i, 2);
    }
    return b;
}

// Similarity according to Sorensen-Dice coefficient
float StringUtils::similarity(const String &lhs,const String &p_string) {
    if (lhs==p_string) {
        // Equal strings are totally similar
        return 1.0f;
    }
    if (lhs.length() < 2 || p_string.length() < 2) {
        // No way to calculate similarity without a single bigram
        return 0.0f;
    }

    Vector<String> src_bigrams = StringUtils::bigrams(lhs);
    Vector<String> tgt_bigrams = StringUtils::bigrams(p_string);

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

bool StringUtils::match(const String &s, const String &p_wildcard, Compare sensitivity)  {

    if (p_wildcard.empty() || s.empty())
        return false;
    assert(sensitivity!=CaseNatural);
    return _wildcard_match(p_wildcard.cdata(), s.cdata(), sensitivity==CaseSensitive);
}

bool StringUtils::matchn(const String &s,const String &p_wildcard)  {
    return match(s,p_wildcard,CaseInsensitive);
}

String StringUtils::format(const String &fmt, const Variant &values) {

    static const String quote_char("\"");
    static const String underscore("_");
    String new_string = String(fmt);

    if (values.get_type() == VariantType::ARRAY) {
        Array values_arr = values;
        for (int i = 0; i < values_arr.size(); i++) {
            String i_as_str = num_int64(i);

            if (values_arr[i].get_type() == VariantType::ARRAY) { //Array in Array structure [["name","RobotGuy"],[0,"godot"],["strength",9000.91]]
                Array value_arr = values_arr[i];

                if (value_arr.size() == 2) {
                    Variant v_key = value_arr[0];
                    String key = v_key.as<String>();
                    if (left(key,1) == quote_char && right(key,key.length() - 1) == quote_char) {
                        key = substr(key,1, key.length() - 2);
                    }

                    Variant v_val = value_arr[1];
                    String val = v_val.as<String>();
                    if (is_enclosed_in(val,'"')) {
                        val = substr(val,1, val.length() - 2);
                    }
                    new_string = StringUtils::replace(new_string,"{"+key+"}", val);
                } else {
                    ERR_PRINT("STRING.format Inner Array size != 2 ")
                }
            } else { //Array structure ["RobotGuy","Logis","rookie"]
                Variant v_val = values_arr[i];
                String val = v_val.as<String>();

                if (is_enclosed_in(val,'"')) {
                    val = StringUtils::substr(val,1, val.length() - 2);
                }

                new_string = replace(new_string,"{"+i_as_str+"}", val);
            }
        }
    } else if (values.get_type() == VariantType::DICTIONARY) {
        Dictionary d = values;
        ListPOD<Variant> keys;
        d.get_key_list(&keys);

        for (const Variant &E : keys) {
            String key = E.as<String>();
            String val = d[E].as<String>();

            if (is_enclosed_in(key,'"')) {
                key = StringUtils::substr(key,1, key.length() - 2);
            }

            if (is_enclosed_in(val,'"')) {
                val = StringUtils::substr(val,1, val.length() - 2);
            }

            new_string = StringUtils::replace(new_string,"{"+key+"}", val);
        }
    } else {
        ERR_PRINT("Invalid type: use Array or Dictionary.")
    }

    return new_string;
}

String StringUtils::replace_first(const String &s,const String &p_key, const String &p_with)  {

    int pos = find(s,p_key);
    String res=s;
    if(pos<0)
        return res;
    return res.m_str.replace(pos,p_key.length(),p_with.m_str);
}
String StringUtils::replacen(const String &s,const String &p_key, const String &p_with) {


    QString new_string=s.m_str;
    return new_string.replace(p_key.m_str,p_with.m_str,Qt::CaseInsensitive);
}
String StringUtils::replace(const String &s,const String &p_key, const String &p_with) {
    QString new_string=s.m_str;
    return new_string.replace(p_key.m_str,p_with.m_str,Qt::CaseSensitive);
}
String StringUtils::replace(const String &s,CharType p_key, CharType p_with) {
    QString new_string=s.m_str;
    return new_string.replace(p_key,p_with,Qt::CaseSensitive);
}
String StringUtils::replace(const String &str,const char *p_key, const char *p_with) {
    QString new_string=str.m_str;
    return new_string.replace(p_key,p_with,Qt::CaseSensitive);
}

void StringUtils::Inplace::replace(String &s,int i,int len, const String &p_after) {
    s.m_str.replace(i,len,p_after.m_str);
}

String StringUtils::repeat(const String &str,int p_count) {

    ERR_FAIL_COND_V_CMSG(p_count < 0, "", "Parameter count should be a positive number.")

    String new_string;

    new_string.m_str.resize(str.length() * p_count + 1);

    for (int i = 0; i < p_count; i++)
        for (int j = 0; j < str.length(); j++)
            new_string.m_str[i * str.length() + j] = str[j];

    return new_string;
}

String StringUtils::left(const String &s,int p_pos) {
    return s.m_str.mid(0, p_pos);
}

String StringUtils::right(const String &s,int p_pos){

    return s.m_str.mid(p_pos);
}

CharType StringUtils::ord_at(const String &str,int p_idx) {

    ERR_FAIL_INDEX_V(p_idx, str.length(), 0)
    return str[p_idx];
}

String StringUtils::dedent(const String &str) {

    String new_string;
    String indent;
    bool has_indent = false;
    bool has_text = false;
    int line_start = 0;
    int indent_stop = -1;

    for (int i = 0; i < str.length(); i++) {

        CharType c = str[i];
        if (c == '\n') {
            if (has_text)
                new_string += StringUtils::substr(str,indent_stop, i - indent_stop);
            new_string += '\n';
            has_text = false;
            line_start = i + 1;
            indent_stop = -1;
        } else if (!has_text) {
            if (c > 32) {
                has_text = true;
                if (!has_indent) {
                    has_indent = true;
                    indent = StringUtils::substr(str,line_start, i - line_start);
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
        new_string += StringUtils::substr(str,indent_stop);

    return String(new_string);
}

String StringUtils::strip_edges(const String &str,bool left, bool right)  {

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

String StringUtils::strip_escapes(const String &str)  {

    QString new_string;
    for (int i = 0; i < str.m_str.length(); i++) {

        // Escape characters on first page of the ASCII table, before 32 (Space).
        if (str[i] < 32)
            continue;
        new_string += str[i];
    }

    return String(new_string);
}

String StringUtils::lstrip(const String &str,const String &p_chars)  {

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

String StringUtils::rstrip(const String &str,const String &p_chars)  {

    int len = str.length();
    int end;

    for (end = len - 1; end >= 0; end--) {

        if (find_char(p_chars,str.m_str[end]) == -1)
            break;
    }

    if (end == len - 1)
        return str;

    return substr(str,0, end + 1);
}

String PathUtils::simplify_path(const String &str) {

    QString s = str.m_str;
    String drive;
    if (s.startsWith("local://")) {
        drive = "local://";
        s = s.mid(8);
    } else if (s.startsWith("res://")) {

        drive = "res://";
        s = s.mid(6);
    } else if (s.startsWith("user://")) {

        drive = "user://";
        s = s.mid(7, s.length());
    } else if (s.startsWith("/") || s.startsWith("\\")) {

        drive = s.mid(0, 1);
        s = s.mid(1);
    } else {

        int p = s.indexOf(":/");
        if (p == -1)
            p = s.indexOf(":\\");
        if (p != -1 && p < s.indexOf("/")) {

            drive = s.mid(0, p + 2);
            s = s.mid(p + 2, s.length());
        }
    }

    s = s.replace('\\', '/');
    while (true) { // in case of using 2 or more slash
        String compare =StringUtils::replace(s,"//", "/");
        if (s == compare.m_str)
            break;
        else
            s = compare.m_str;
    }
    auto dirs = s.splitRef("/", QString::SkipEmptyParts);

    for (int i = 0; i < dirs.size(); i++) {

        QStringRef d = dirs[i];
        if (d == ".") {
            dirs.remove(i);
            i--;
        } else if (d == "..") {

            if (i == 0) {
                dirs.remove(i);
                i--;
            } else {
                dirs.remove(i);
                dirs.remove(i - 1);
                i -= 2;
            }
        }
    }

    QString res = "";

    for (int i = 0; i < dirs.size(); i++) {

        if (i > 0)
            res += "/";
        res += dirs[i];
    }

    return String(drive.m_str + res);
}

static int _humanize_digits(int p_num) {

    if (p_num < 10)
        return 2;
    else if (p_num < 100)
        return 2;
    else if (p_num < 1024)
        return 1;
    else
        return 0;
}

String PathUtils::humanize_size(size_t p_size) {

    uint64_t _div = 1;
    static const char *prefix[] = { " B", " KiB", " MiB", " GiB", " TiB", " PiB", " EiB", "" };
    int prefix_idx = 0;

    while (p_size > (_div * 1024) && prefix[prefix_idx][0]) {
        _div *= 1024;
        prefix_idx++;
    }

    int digits = prefix_idx > 0 ? _humanize_digits(p_size / _div) : 0;
    double divisor = prefix_idx > 0 ? _div : 1;

    return StringUtils::pad_decimals(StringUtils::num(p_size / divisor),digits) + RTR(prefix[prefix_idx]);
}
bool PathUtils::is_abs_path(const String &str) {

    if (str.length() > 1)
        return (str[0] == '/' || str[0] == '\\' || str.m_str.contains(":/") || str.m_str.contains(":\\"));
    else if (str.length() == 1)
        return (str[0] == '/' || str[0] == '\\');
    else
        return false;
}

bool StringUtils::is_valid_identifier(const String &str) {

    int len =str.length();

    if (len == 0)
        return false;

    for (int i = 0; i < len; i++) {

        if (i == 0) {
            if (str.m_str.front() >= '0' && str.m_str.front() <= '9')
                return false; // no start with number plz
        }
        QChar c = str[i];
        bool valid_char = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';

        if (!valid_char)
            return false;
    }

    return true;
}

//kind of poor should be rewritten properly

String StringUtils::word_wrap(const String &str,int p_chars_per_line) {

    int from = 0;
    int last_space = 0;
    String ret;
    for (int i = 0; i < str.length(); i++) {
        if (i - from >= p_chars_per_line) {
            if (last_space == -1) {
                ret += StringUtils::substr(str,from, i - from + 1) + "\n";
            } else {
                ret += StringUtils::substr(str,from, last_space - from) + "\n";
                i = last_space; //rewind
            }
            from = i + 1;
            last_space = -1;
        } else if (str[i] == ' ' || str[i] == '\t') {
            last_space = i;
        } else if (str[i] == '\n') {
            ret += StringUtils::substr(str,from, i - from) + "\n";
            from = i + 1;
            last_space = -1;
        }
    }

    if (from < str.length()) {
        ret += StringUtils::substr(str,from);
    }

    return String(ret);
}

String StringUtils::http_escape(const String &s) {
    const CharString temp = s.m_str.toUtf8();
    QString res;
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
    return String(res);
}

String StringUtils::http_unescape(const String &str) {
    QString res;
    for (int i = 0; i < str.length(); ++i) {
        if (str.m_str.at(i) == '%' && i + 2 < str.length()) {
            CharType ord1 = str.m_str.at(i + 1);
            if ((ord1 >= '0' && ord1 <= '9') || (ord1 >= 'A' && ord1 <= 'Z')) {
                CharType ord2 = str.m_str.at(i + 2);
                if ((ord2 >= '0' && ord2 <= '9') || (ord2 >= 'A' && ord2 <= 'Z')) {
                    char bytes[3] = { (char)ord1.toLatin1(), (char)ord2.toLatin1(), 0 };
                    res += (char)strtol(bytes, nullptr, 16);
                    i += 2;
                }
            } else {
                res += str.m_str.at(i);
            }
        } else {
            res += str.m_str.at(i);
        }
    }
    return StringUtils::from_utf8(StringUtils::ascii(res));
}

String StringUtils::c_unescape(const String &str) {

    String escaped = str;
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

String StringUtils::c_escape(const String &e) {

    String escaped = e;
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

String StringUtils::c_escape_multiline(const String &str) {

    String escaped = str;
    escaped = StringUtils::replace(escaped,"\\", "\\\\");
    escaped = StringUtils::replace(escaped,"\"", "\\\"");

    return escaped;
}

String StringUtils::json_escape(const String &str) {

    String escaped = str;
    escaped = StringUtils::replace(escaped,"\\", "\\\\");
    escaped = StringUtils::replace(escaped,"\b", "\\b");
    escaped = StringUtils::replace(escaped,"\f", "\\f");
    escaped = StringUtils::replace(escaped,"\n", "\\n");
    escaped = StringUtils::replace(escaped,"\r", "\\r");
    escaped = StringUtils::replace(escaped,"\t", "\\t");
    escaped = StringUtils::replace(escaped,"\v", "\\v");
    escaped = StringUtils::replace(escaped,"\"", "\\\"");

    return escaped;
}

String StringUtils::xml_escape(const String &arg,bool p_escape_quotes)  {

    String str = arg;
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

String StringUtils::xml_unescape(const String &arg) {

    String str;
    int l = arg.length();
    int len = _xml_unescape(arg.cdata(), l, nullptr);
    if (len == 0)
        return String();
    str.m_str.resize(len);
    _xml_unescape(arg.cdata(), l, str.data());

    return str;
}

String StringUtils::pad_decimals(const String &str,int p_digits) {

    String s = str;
    int c = s.m_str.indexOf(".");

    if (c == -1) {
        if (p_digits <= 0) {
            return s;
        }
        s += '.';
        c = s.length() - 1;
    } else {
        if (p_digits <= 0) {
            return StringUtils::substr(s,0, c);
        }
    }

    if (s.length() - (c + 1) > p_digits) {
        s = StringUtils::substr(s, 0, c + p_digits + 1);
    } else {
        while (s.length() - (c + 1) < p_digits) {
            s += '0';
        }
    }
    return s;
}

String StringUtils::pad_zeros(const String &src,int p_digits) {

    String s = src;
    int end = StringUtils::find(s,".");

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

        s = s.m_str.insert(begin, "0");
        end++;
    }

    return s;
}

String StringUtils::trim_prefix(const String &src,const String &p_prefix) {

    String s = src;
    if (begins_with(s,p_prefix)) {
        return substr(s,p_prefix.length(), s.length() - p_prefix.length());
    }
    return s;
}

String StringUtils::trim_suffix(const String &src,const String &p_suffix) {

    String s = src;
    if (ends_with(s,p_suffix)) {
        return substr(s,0, s.length() - p_suffix.length());
    }
    return s;
}

bool StringUtils::is_valid_integer(const String &str) {

    int len = str.length();

    if (len == 0)
        return false;

    int from = 0;
    if (len != 1 && (str.m_str.front() == '+' || str.m_str.front() == '-'))
        from++;

    for (int i = from; i < len; i++) {

        if (!str[i].isDigit())
            return false; // no start with number plz
    }

    return true;
}

bool StringUtils::is_valid_hex_number(const String &str,bool p_with_prefix) {

    int len = str.length();

    if (len == 0)
        return false;

    int from = 0;
    if (len != 1 && (str.m_str.front() == '+' || str.m_str.front() == '-'))
        from++;

    if (p_with_prefix) {

        if (len < 3)
            return false;
        if (str[from] != '0' || str[from + 1] != 'x') {
            return false;
        }
        from += 2;
    }

    for (int i = from; i < len; i++) {

        CharType c = str[i];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
            continue;
        return false;
    }

    return true;
};

bool StringUtils::is_valid_float(const String &str) {

    int len = str.length();

    if (len == 0)
        return false;

    int from = 0;
    if (str.m_str.front() == '+' || str.m_str.front() == '-') {
        from++;
    }

    bool exponent_found = false;
    bool period_found = false;
    bool sign_found = false;
    bool exponent_values_found = false;
    bool numbers_found = false;

    for (int i = from; i < len; i++) {

        if (str[i].isDigit()) {

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

String PathUtils::path_to_file(const String &base,String p_path) {

    String src = get_base_dir(PathUtils::from_native_path(base));
    String dst = get_base_dir(PathUtils::from_native_path(p_path));
    String rel = path_to(src,dst);
    if (rel == dst) // failed
        return p_path;

    return rel + PathUtils::get_file(p_path);
}

String PathUtils::path_to(const String &str,String p_path) {

    QString src = PathUtils::from_native_path(str).m_str;
    QString dst = PathUtils::from_native_path(p_path).m_str;
    if (!src.endsWith("/"))
        src += "/";
    if (!dst.endsWith("/"))
        dst += "/";

    String base;

    if (src.startsWith("res://") && dst.startsWith("res://")) {

        base = "res:/";
        src.replace("res://", "/");
        dst.replace("res://", "/");

    } else if (src.startsWith("user://") && dst.startsWith("user://")) {

        base = "user:/";
        src.replace("user://", "/");
        dst.replace("user://", "/");

    } else if (src.startsWith("/") && dst.startsWith("/")) {

        //nothing
    } else {
        //dos style
        String src_begin = StringUtils::get_slice(src,'/', 0);
        String dst_begin = StringUtils::get_slice(dst,'/', 0);

        if (src_begin != dst_begin)
            return p_path; //impossible to do this

        base = src_begin;
        src = src.mid(src_begin.length(), src.length());
        dst = dst.mid(dst_begin.length(), dst.length());
    }

    //remove leading and trailing slash and split
    auto src_dirs = src.mid(1, src.length() - 2).splitRef("/");
    auto dst_dirs = dst.mid(1, dst.length() - 2).splitRef("/");

    //find common parent
    int common_parent = 0;

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

    QString dir;

    for (int i = src_dirs.size() - 1; i > common_parent; i--) {

        dir += "../";
    }

    for (int i = common_parent + 1; i < dst_dirs.size(); i++) {

        dir += dst_dirs[i] + "/";
    }

    if (dir.length() == 0)
        dir = "./";
    return String(dir);
}

bool StringUtils::is_valid_html_color(const String &str) {

    return Color::html_is_valid(str);
}

bool StringUtils::is_valid_filename(const String &str) {

    String stripped =StringUtils::strip_edges( str);
    if (str != stripped) {
        return false;
    }

    if (stripped.empty()) {
        return false;
    }
    //TODO: SEGS: convert this chain of string scans to something saner.
    return !(str.m_str.contains(":") || str.m_str.contains('/') || str.m_str.contains('\\') || str.m_str.contains('?') ||
             str.m_str.contains('*') || str.m_str.contains('\"') || str.m_str.contains('|') || str.m_str.contains('%') || str.m_str.contains('<') ||
             str.m_str.contains('>'));
}

bool StringUtils::is_valid_ip_address(const String &str) {

    if (StringUtils::contains(str,':')) {

        Vector<String> ip = split(str,':');
        for (int i = 0; i < ip.size(); i++) {

            String n = ip[i];
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
        Vector<String> ip = split(str,'.');
        if (ip.size() != 4)
            return false;
        for (int i = 0; i < ip.size(); i++) {

            String n = ip[i];
            if (!is_valid_integer(n))
                return false;
            int val = to_int(n);
            if (val < 0 || val > 255)
                return false;
        }
    }

    return true;
}

bool PathUtils::is_resource_file(const String &str) {

    return StringUtils::begins_with(str,"res://") && !str.m_str.contains("::");
}

bool PathUtils::is_rel_path(const String &str) {

    return !is_abs_path(str);
}
String PathUtils::trim_trailing_slash(const String &path) {
    CharType last_char = path.back();
    if(last_char=='/' || last_char=='\\')
        return StringUtils::substr(path,0,path.size()-1);
    return path;
}
String PathUtils::get_base_dir(const String &path) {

    int basepos = StringUtils::find(path,"://");
    String rs;
    String base;
    if (basepos != -1) {
        int end = basepos + 3;
        rs = StringUtils::substr(path,end);
        base = StringUtils::substr(path,0, end);
    } else {
        if (path.m_str.startsWith('/')) {
            rs = StringUtils::substr(path,1);
            base = "/";
        } else {

            rs = path;
        }
    }

    int sep = std::max(StringUtils::find_last(rs,'/'), StringUtils::find_last(rs,'\\'));
    if (sep == -1)
        return base;

    return base + StringUtils::substr(rs,0, sep);
}

String PathUtils::get_file(const String &path) {

    int sep = MAX(path.m_str.lastIndexOf('/'), path.m_str.lastIndexOf('\\'));
    if (sep == -1)
        return path;

    return path.m_str.mid(sep + 1);
}

String PathUtils::get_extension(const String &path) {

    int pos = path.m_str.lastIndexOf(".");
    if (pos < 0 || pos < MAX(path.m_str.lastIndexOf("/"), path.m_str.lastIndexOf("\\")))
        return String();

    return StringUtils::substr(path,pos + 1);
}

String PathUtils::plus_file(const String &bp,const String &p_file) {
    if (bp.empty())
        return p_file;
    if (bp.m_str.back() == '/' || StringUtils::begins_with(p_file,'/'))
        return bp + p_file;
    return bp + "/" + p_file;
}

String StringUtils::percent_encode(const String &str) {

    CharString cs = StringUtils::to_utf8(str);
    String encoded;
    for (int i = 0; i < cs.length(); i++) {
        char c = cs[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '~' || c == '.') {
            encoded += c;
        } else {
            char p[4] = { '%', 0, 0, 0 };
            static const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

            p[1] = hex[c >> 4];
            p[2] = hex[c & 0xF];
            encoded.m_str.append(p);
        }
    }

    return encoded;
}
String StringUtils::percent_decode(const String &str) {

    CharString pe;

    CharString cs = StringUtils::to_utf8(str);
    for (int i = 0; i < cs.length(); i++) {

        uint8_t c = cs[i];
        if (c == '%' && i < str.length() - 2) {

            uint8_t a = tolower(cs[i + 1]);
            uint8_t b = tolower(cs[i + 2]);

            if (a >= '0' && a <= '9')
                c = (a - '0') << 4;
            else if (a >= 'a' && a <= 'f')
                c = (a - 'a' + 10) << 4;
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
    String a=QString::fromUtf8(pe);
    return a;
}

String PathUtils::get_basename(const String &path) {

    int pos = path.m_str.lastIndexOf('.');
    if (pos < 0 || pos < MAX(path.m_str.lastIndexOf("/"), path.m_str.lastIndexOf("\\")))
        return path;

    return StringUtils::substr(path,0, pos);
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
String StringUtils::rpad(const String &src,int min_length, CharType character)  {
    String s = src;
    int padding = min_length - s.length();
    if (padding > 0) {
        for (int i = 0; i < padding; i++)
            s = s + character;
    }

    return s;
}
// Left-pad with a character.
String StringUtils::lpad(const String &src,int min_length, CharType character)  {
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
String StringUtils::sprintf(const String &str,const Array &values, bool *error) {
    String formatted;
    const CharType *self = (CharType *)str.cdata();
    bool in_format = false;
    int value_index = 0;
    int min_chars = 0;
    int min_decimals = 0;
    bool in_decimals = false;
    bool pad_with_zeroes = false;
    bool left_justified = false;
    bool show_sign = false;

    *error = true;

    for (; !self->isNull(); self++) {
        const CharType c = *self;

        if (in_format) { // We have % - lets see what else we get.
            switch (c.toLatin1()) {
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
                        return String("not enough arguments for format string");
                    }

                    if (!values[value_index].is_num()) {
                        return String("a number is required");
                    }

                    int64_t value = values[value_index];
                    int base = 16;
                    bool capitalize = false;
                    switch (c.toLatin1()) {
                        case 'd': base = 10; break;
                        case 'o': base = 8; break;
                        case 'x': break;
                        case 'X':
                            base = 16;
                            capitalize = true;
                            break;
                    }
                    // Get basic number.
                    String str = StringUtils::num_int64(ABS(value), base, capitalize);
                    int number_len = str.length();

                    // Padding.
                    CharType pad_char = pad_with_zeroes ? '0' : ' ';
                    if (left_justified) {
                        str = StringUtils::rpad(str,min_chars, pad_char);
                    } else {
                        str = StringUtils::lpad(str,min_chars, pad_char);
                    }

                    // Sign.
                    if (show_sign && value >= 0) {
                        str = str.m_str.insert(pad_with_zeroes ? 0 : str.length() - number_len, "+");
                    } else if (value < 0) {
                        str = str.m_str.insert(pad_with_zeroes ? 0 : str.length() - number_len, "-");
                    }

                    formatted += str;
                    ++value_index;
                    in_format = false;

                    break;
                }
                case 'f': { // Float
                    if (value_index >= values.size()) {
                        return String("not enough arguments for format string");
                    }

                    if (!values[value_index].is_num()) {
                        return String("a number is required");
                    }

                    double value = values[value_index];
                    String str = StringUtils::num(value, min_decimals);

                    // Pad decimals out.
                    str = pad_decimals(str,min_decimals);

                    // Show sign
                    if (show_sign && StringUtils::left(str,1) != "-") {
                        str = str.m_str.insert(0, "+");
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
                        return String("not enough arguments for format string");
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
                        return String("not enough arguments for format string");
                    }

                    // Convert to character.
                    String str;
                    if (values[value_index].is_num()) {
                        int value = values[value_index];
                        if (value < 0) {
                            return String("unsigned byte integer is lower than maximum");
                        } else if (value > 255) {
                            return String("unsigned byte integer is greater than maximum");
                        }
                        str = String(values[value_index].as<QChar>());
                    } else if (values[value_index].get_type() == VariantType::STRING) {
                        str = values[value_index].as<String>();
                        if (str.length() != 1) {
                            return String("%c requires number or single-character string");
                        }
                    } else {
                        return String("%c requires number or single-character string");
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
                    int n = c.digitValue();
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
                        return String("too many decimal points in format");
                    }
                    in_decimals = true;
                    min_decimals = 0; // We want to add the value manually.
                    break;
                }

                case '*': { // Dynamic width, based on value.
                    if (value_index >= values.size()) {
                        return String("not enough arguments for format string");
                    }

                    if (!values[value_index].is_num()) {
                        return String("* wants number");
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
                    return String("unsupported format character");
                }
            }
        } else { // Not in format string.
            switch (c.toLatin1()) {
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
        return String("incomplete format");
    }

    if (value_index != values.size()) {
        return String("not all arguments converted during string formatting");
    }

    *error = false;
    return formatted;
}

String StringUtils::quote(const String &str,CharType character) {
    return character + str + character;
}

String StringUtils::unquote(const String &str) {
    if (!StringUtils::is_quoted(str)) {
        return str;
    }

    return StringUtils::substr(str,1, str.length() - 2);
}

#ifdef TOOLS_ENABLED
String TTR(const String &p_text) {

    if (TranslationServer::get_singleton()) {
        return TranslationServer::get_singleton()->tool_translate(p_text);
    }

    return p_text;
}

#endif

String RTR(const String &p_text) {

    if (TranslationServer::get_singleton()) {
        String rtr = TranslationServer::get_singleton()->tool_translate(p_text);
        if (rtr.empty() || rtr == p_text) {
            return TranslationServer::get_singleton()->translate(p_text);
        } else {
            return rtr;
        }
    }

    return p_text;
}

int StringUtils::compare(const String &lhs, const String &rhs, Compare case_sensitive)
{
    if(case_sensitive==CaseSensitive)
    {
        return lhs.m_str.compare(rhs.m_str,Qt::CaseSensitive);
    } else if(case_sensitive==CaseInsensitive)
    {
        return lhs.m_str.compare(rhs.m_str,Qt::CaseInsensitive);
    }
    QCollator col;
    col.setNumericMode(true);
    return col.compare(lhs.m_str,rhs.m_str);
}

bool StringUtils::contains(const char *heystack, const char *needle, Compare mode)
{
    std::string_view sv1(heystack);
    std::string_view nd1(needle);
    return sv1.find(nd1)!=std::string_view::npos;
}
bool StringUtils::contains(const String &heystack, const String &needle,Compare mode)
{
    assert(mode!=Compare::CaseNatural);
    return heystack.m_str.contains(needle.m_str,mode==CaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
}
bool StringUtils::contains(const String &heystack, CharType c, Compare)
{
    return heystack.m_str.contains(c);
}

bool PathUtils::is_internal_path(const String &path)
{
    return StringUtils::contains(path,"local://") || StringUtils::contains(path,"::");
}

String StringUtils::from_utf8(const char *p_utf8, int p_len)
{
    String res;
    res.m_str = QString::fromUtf8(p_utf8,p_len);
    return res;
}

String StringUtils::from_wchar(const wchar_t *p_utf8, int p_len)
{
    return String { QString::fromWCharArray(p_utf8,p_len)};
}

void StringUtils::Inplace::replace(String &str, const char *p_key, const char *p_with)
{
    str.m_str.replace(p_key,p_with);
}

void StringUtils::Inplace::replace(String &str, CharType p_key, CharType p_with)
{
    str.m_str.replace(p_key,p_with);
}

void StringUtils::Inplace::replace(String &str, const String &p_key, const String &p_with)
{
    str.m_str.replace(p_key.m_str,p_with.m_str);
}
