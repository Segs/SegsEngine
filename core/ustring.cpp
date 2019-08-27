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
#include "core/print_string.h"
#include "core/translation.h"
#include "core/ucaps.h"
#include "core/variant.h"

#include <wchar.h>
#include <QString>
#include <QVector>

#ifndef NO_USE_STDLIB
#include <QCollator>
#include <stdio.h>
#include <stdlib.h>
#endif

#if defined(MINGW_ENABLED) || defined(_MSC_VER)
#define snprintf _snprintf_s
#endif

#define MAX_DIGITS 6
namespace
{
bool is_enclosed_in(const String &str,const String &p_string) {

    return str.begins_with(p_string) && str.ends_with(p_string);
}

}
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
    resize(p_length + 1);
    set(p_length, 0);

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
    const CharType *dst = constData();

    /* Compare char by char */
    for (int i = 0; i < len; i++) {

        if (c_str[i] != dst[i])
            return false;
    }

    return true;
}

void String::erase(int p_pos, int p_chars) {

    *this = left(p_pos) + substr(p_pos + p_chars, length() - ((p_pos + p_chars)));
}

String String::capitalize() const {

    String aux = String(camelcase_to_underscore(true).replace("_", " ")).strip_edges();
    String cap;
    for (int i = 0; i < aux.get_slice_count(" "); i++) {

        String slice = aux.get_slicec(' ', i);
        if (slice.length() > 0) {

            slice[0] = slice[0].toUpper();
            if (i > 0)
                cap += " ";
            cap += slice;
        }
    }

    return cap;
}

String String::camelcase_to_underscore(bool lowercase) const {
    const CharType *cstr = constData();
    String new_string;
    const char A = 'A', Z = 'Z';
    const char a = 'a', z = 'z';
    int start_index = 0;

    for (int i = 1; i < this->size(); i++) {
        bool is_upper = cstr[i] >= A && cstr[i] <= Z;
        bool is_number = cstr[i] >= '0' && cstr[i] <= '9';
        bool are_next_2_lower = false;
        bool is_next_lower = false;
        bool is_next_number = false;
        bool was_precedent_upper = cstr[i - 1] >= A && cstr[i - 1] <= Z;
        bool was_precedent_number = cstr[i - 1] >= '0' && cstr[i - 1] <= '9';

        if (i + 2 < this->size()) {
            are_next_2_lower = cstr[i + 1] >= a && cstr[i + 1] <= z && cstr[i + 2] >= a && cstr[i + 2] <= z;
        }

        if (i + 1 < this->size()) {
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
            new_string += this->substr(start_index, i - start_index) + "_";
            start_index = i;
        }
    }

    new_string += this->substr(start_index, this->size() - start_index);
    return lowercase ? StringUtils::to_lower(new_string) : new_string;
}

int String::get_slice_count(String p_splitter) const {

    if (empty())
        return 0;
    if (p_splitter.empty())
        return 0;

    int pos = 0;
    int slices = 1;

    while ((pos = find(p_splitter, pos)) >= 0) {

        slices++;
        pos += p_splitter.length();
    }

    return slices;
}

String String::get_slice(String p_splitter, int p_slice) const {

    if (empty() || p_splitter.empty())
        return "";

    int pos = 0;
    int prev_pos = 0;
    //int slices=1;
    if (p_slice < 0)
        return "";
    if (find(p_splitter) == -1)
        return *this;

    int i = 0;
    while (true) {

        pos = find(p_splitter, pos);
        if (pos == -1)
            pos = length(); //reached end

        int from = prev_pos;
        //int to=pos;

        if (p_slice == i) {

            return substr(from, pos - from);
        }

        if (pos == length()) //reached end and no find
            break;
        pos += p_splitter.length();
        prev_pos = pos;
        i++;
    }

    return ""; //no find!
}

String String::get_slicec(CharType p_splitter, int p_slice) const {

    if (empty())
        return String();

    if (p_slice < 0)
        return String();

    const CharType *c = this->cdata();
    int i = 0;
    int prev = 0;
    int count = 0;
    while (true) {

        if (c[i] == 0 || c[i] == p_splitter) {

            if (p_slice == count) {

                return substr(prev, i - prev);
            } else if (c[i] == 0) {
                return String();
            } else {
                count++;
                prev = i + 1;
            }
        }

        i++;
    }
}

Vector<String> StringUtils::split_spaces(const String &str) {

    Vector<String> ret;
    int from = 0;
    int i = 0;
    int len = str.length();
    if (len == 0)
        return ret;

    bool inside = false;

    while (true) {

        bool empty = str[i] < 33;

        if (i == 0)
            inside = !empty;

        if (!empty && !inside) {
            inside = true;
            from = i;
        }

        if (empty && inside) {

            ret.push_back(str.mid(from, i - from));
            inside = false;
        }

        if (i == len)
            break;
        i++;
    }

    return ret;
}

Vector<String> StringUtils::split(const String &str,const String &p_splitter, bool p_allow_empty, int p_maxsplit) {

    Vector<String> ret;
    int from = 0;
    int len = str.length();

    while (true) {

        int end = str.find(p_splitter, from);
        if (end < 0)
            end = len;
        if (p_allow_empty || (end > from)) {
            if (p_maxsplit <= 0)
                ret.push_back(str.mid(from, end - from));
            else {

                // Put rest of the string and leave cycle.
                if (p_maxsplit == ret.size()) {
                    ret.push_back(str.mid(from));
                    break;
                }

                // Otherwise, push items until positive limit is reached.
                ret.push_back(str.mid(from, end - from));
            }
        }

        if (end == len)
            break;

        from = end + p_splitter.length();
    }

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
                ret.push_back(str.mid(0, remaining_len));
            }
            break;
        }

        int left_edge = str.rfind(p_splitter, remaining_len - p_splitter.length());

        if (left_edge < 0) {
            // no more splitters, we're done
            ret.push_back(str.mid(0, remaining_len));
            break;
        }

        int substr_start = left_edge + p_splitter.length();
        if (p_allow_empty || substr_start < remaining_len) {
            ret.push_back(str.mid(substr_start, remaining_len - substr_start));
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

        int end = str.find(p_splitter, from);
        if (end < 0)
            end = len;
        if (p_allow_empty || (end > from))
            ret.push_back(StringUtils::to_double(str.constData()+from));

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
        int end = str.findmk(p_splitters, from, &idx);
        int spl_len = 1;
        if (end < 0) {
            end = len;
        } else {
            spl_len = p_splitters[idx].length();
        }

        if (p_allow_empty || (end > from)) {
            ret.push_back(StringUtils::to_double(str.constData()+from));
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

        int end = str.find(p_splitter, from);
        if (end < 0)
            end = len;
        if (p_allow_empty || (end > from))
            ret.push_back(StringUtils::to_int(str.constData()+from, end - from));

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
        int end = str.findmk(p_splitters, from, &idx);
        int spl_len = 1;
        if (end < 0) {
            end = len;
        } else {
            spl_len = p_splitters[idx].length();
        }

        if (p_allow_empty || (end > from))
            ret.push_back(StringUtils::to_int(str.constData()+from, end - from));

        if (end == len)
            break;

        from = end + spl_len;
    }

    return ret;
}

String StringUtils::join(const String &str,Vector<String> parts) {
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

    return p_char.toUpper();
}

CharType StringUtils::char_lowercase(CharType p_char) {

    return p_char.toLower();
}

String StringUtils::to_upper(const String &str){

    return str.toUpper();
}

String StringUtils::to_lower(const String &str) {
    return str.toLower();
}

String StringUtils::md5(const uint8_t *p_md5) {
    return StringUtils::hex_encode_buffer(p_md5, 16);
}

String StringUtils::hex_encode_buffer(const uint8_t *p_buffer, int p_len) {
    static const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

    String ret;
    char v[2] = { 0, 0 };

    for (int i = 0; i < p_len; i++) {
        v[0] = hex[p_buffer[i] >> 4];
        ret += v;
        v[0] = hex[p_buffer[i] & 0xF];
        ret += v;
    }

    return ret;
}

String StringUtils::num(double p_num, int p_decimals) {

#ifndef NO_USE_STDLIB

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
#else

    String s;
    String sd;
    /* integer part */

    bool neg = p_num < 0;
    p_num = ABS(p_num);
    int intn = (int)p_num;

    /* decimal part */

    if (p_decimals > 0 || (p_decimals == -1 && (int)p_num != p_num)) {

        double dec = p_num - (float)((int)p_num);

        int digit = 0;
        if (p_decimals > MAX_DIGITS)
            p_decimals = MAX_DIGITS;

        int dec_int = 0;
        int dec_max = 0;

        while (true) {

            dec *= 10.0;
            dec_int = dec_int * 10 + (int)dec % 10;
            dec_max = dec_max * 10 + 9;
            digit++;

            if (p_decimals == -1) {

                if (digit == MAX_DIGITS) //no point in going to infinite
                    break;

                if ((dec - (float)((int)dec)) < 1e-6)
                    break;
            }

            if (digit == p_decimals)
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
#endif
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
    s.resize(chars + 1);
    CharType *c = s.data();
    c[chars] = 0;
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
    s.resize(chars + 1);
    CharType *c = s.data();
    c[chars] = 0;
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

#ifndef NO_USE_STDLIB

    char buf[256];

#if defined(__GNUC__) || defined(_MSC_VER)
    snprintf(buf, 256, "%lg", p_num);
#else
    sprintf(buf, "%.16lg", p_num);
#endif

    buf[255] = 0;

    return buf;
#else

    return String::num(p_num);
#endif
}

CharString String::ascii(bool p_allow_extended) const {
    if(p_allow_extended)
        return toLocal8Bit();
    else
        return toLatin1();
}

String String::utf8(const char *p_utf8, int p_len) {

    String ret;
    ret.parse_utf8(p_utf8, p_len);

    return ret;
};

bool String::parse_utf8(const char *p_utf8, int p_len) {

    if (!p_utf8)
        return true;

    *this = QString::fromUtf8(p_utf8, p_len);
    return !isEmpty();
}

CharString String::utf8() const {
    return toUtf8();
}

String::String(const StrRange &p_range) {

    if (!p_range.c_str)
        return;
    this->append(p_range.c_str, p_range.len);
}

int String::hex_to_int(bool p_with_prefix) const {

    if (p_with_prefix && length() < 3)
        return 0;
    QStringRef to_convert;
    if (p_with_prefix) {
        if (!startsWith("0x"))
            return 0;
         to_convert = midRef(2);
    }
    else
        to_convert = midRef(0);
    return to_convert.toInt(nullptr,16);
}

int64_t String::hex_to_int64(bool p_with_prefix) const {

    if (p_with_prefix && length() < 3)
        return 0;
    QStringRef to_convert;
    if (p_with_prefix) {
        if (!startsWith("0x"))
            return 0;
         to_convert = midRef(2);
    }
    else
        to_convert = midRef(0);
    return to_convert.toLongLong(nullptr,16);
}

int64_t String::bin_to_int64(bool p_with_prefix) const {

    if (p_with_prefix && length() < 3)
        return 0;
    QStringRef to_convert;
    if (p_with_prefix) {
        if (!startsWith("0b"))
            return 0;
         to_convert = midRef(2);
    }
    else
        to_convert = midRef(0);
    return to_convert.toLongLong(nullptr,2);
}

int64_t String::to_int64() const {
    return toLongLong();
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

float String::to_float() const {
    return toFloat();
}

double StringUtils::to_double(const CharType *p_str, const CharType **r_end) {
    return built_in_strtod(p_str, r_end);
}

int64_t StringUtils::to_int(const CharType *p_str, int p_len) {

    if (p_len == 0 || p_str[0].isNull())
        return 0;
    return QString::fromRawData(p_str,p_len).toLongLong();
}

double String::to_double() const {
    return toDouble();
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

String String::insert(int p_at_pos, const String &p_string) const {
    return String(*this).insert(p_at_pos,p_string.m_contents);
}
String String::substr(int p_from, int p_chars) const {

    if ((p_from + p_chars) > length()) {

        p_chars = length() - p_from;
    }
    return m_contents.mid(p_from,p_chars);
}

int String::find_last(const String &p_str) const {

    int pos = -1;
    int findfrom = 0;
    int findres = -1;
    while ((findres = find(p_str, findfrom)) != -1) {

        pos = findres;
        findfrom = pos + 1;
    }

    return pos;
}

int String::find(const String &p_str, int p_from) const {
    if (p_from < 0)
        return -1;
    return m_contents.indexOf(p_str.m_contents,p_from);
}

int String::find(const char *p_str, int p_from) const {

    if (p_from < 0)
        return -1;
    return indexOf(QLatin1String(p_str),p_from);
}

int String::find_char(const CharType &p_char, int p_from) const {
    return indexOf(p_char, p_from);
}

int String::findmk(const Vector<String> &p_keys, int p_from, int *r_key) const {

    if (p_from < 0)
        return -1;
    if (p_keys.size() == 0)
        return -1;

    //int src_len=p_str.length();
    const String *keys = &p_keys[0];
    int key_count = p_keys.size();
    int len = length();

    if (len == 0)
        return -1; // won't find anything!

    const CharType *src = constData();

    for (int i = p_from; i < len; i++) {

        bool found = true;
        for (int k = 0; k < key_count; k++) {

            found = true;
            if (r_key)
                *r_key = k;
            const CharType *cmp = keys[k].constData();
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

int String::findn(const String &p_str, int p_from) const {

    if (p_from < 0)
        return -1;

    int src_len = p_str.length();

    if (src_len == 0 || length() == 0)
        return -1; // won't find anything!

    const CharType *srcd = constData();

    for (int i = p_from; i <= (length() - src_len); i++) {

        bool found = true;
        for (int j = 0; j < src_len; j++) {

            int read_pos = i + j;

            if (read_pos >= length()) {

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

int String::rfind(const String &p_str, int p_from) const {
    return lastIndexOf(p_str,p_from);
}
int String::rfindn(const String &p_str, int p_from) const {
    return lastIndexOf(p_str,p_from,Qt::CaseInsensitive);
}

bool String::ends_with(const String &p_string) const {

    int pos = find_last(p_string);
    if (pos == -1)
        return false;
    return pos + p_string.length() == length();
}

bool String::begins_with(const String &p_string) const {
    return startsWith(p_string);
}
bool String::begins_with(const char *p_string) const {
    return startsWith(p_string);
}

bool StringUtils::is_subsequence_of(const String &str,const String &p_string) {

    return p_string.startsWith(str,Qt::CaseSensitive);
}

bool StringUtils::is_subsequence_ofi(const String &str,const String &p_string) {

    return p_string.startsWith(str,Qt::CaseInsensitive);
}

bool StringUtils::is_quoted(const String &str) {

    return is_enclosed_in(str,"\"") || is_enclosed_in(str,"'");
}

int String::_count(const String &p_string, int p_from, int p_to, bool p_case_insensitive) const {
    if (p_string.empty()) {
        return 0;
    }
    int len = length();
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
            str = String();
            str.copy_from_unchecked(&constData()[0], len);
        } else {
            str = substr(p_from, p_to - p_from);
        }
    } else {
        return 0;
    }
    int c = 0;
    int idx = -1;
    do {
        idx = p_case_insensitive ? str.findn(p_string) : str.find(p_string);
        if (idx != -1) {
            str = str.substr(idx + slen, str.length() - slen);
            ++c;
        }
    } while (idx != -1);
    return c;
}


int String::count(const String &p_string, int p_from, int p_to) const {
return _count(p_string, p_from, p_to, false);
}

int String::countn(const String &p_string, int p_from, int p_to) const {
    return _count(p_string, p_from, p_to, true);
}

Vector<String> StringUtils::bigrams(const String &str) {
    int n_pairs = str.length() - 1;
    Vector<String> b;
    if (n_pairs <= 0) {
        return b;
    }
    b.resize(n_pairs);
    for (int i = 0; i < n_pairs; i++) {
        b.write[i] = str.substr(i, 2);
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

bool String::match(const String &p_wildcard) const {

    if (!p_wildcard.length() || !length())
        return false;

    return _wildcard_match(p_wildcard.constData(), constData(), true);
}

bool String::matchn(const String &p_wildcard) const {

    if (!p_wildcard.length() || !length())
        return false;
    return _wildcard_match(p_wildcard.constData(), constData(), false);
}

String String::format(const Variant &values, const char *placeholder) const {

    String new_string = String(this->cdata());

    if (values.get_type() == Variant::ARRAY) {
        Array values_arr = values;

        for (int i = 0; i < values_arr.size(); i++) {
            String i_as_str = StringUtils::num_int64(i);

            if (values_arr[i].get_type() == Variant::ARRAY) { //Array in Array structure [["name","RobotGuy"],[0,"godot"],["strength",9000.91]]
                Array value_arr = values_arr[i];

                if (value_arr.size() == 2) {
                    Variant v_key = value_arr[0];
                    String key = v_key.as<String>();
                    if (key.left(1) == "\"" && key.right(key.length() - 1) == "\"") {
                        key = key.substr(1, key.length() - 2);
                    }

                    Variant v_val = value_arr[1];
                    String val = v_val.as<String>();

                    if (val.left(1) == "\"" && val.right(val.length() - 1) == "\"") {
                        val = val.substr(1, val.length() - 2);
                    }

                    new_string = new_string.replace(String(placeholder).replace("_", key), val);
                } else {
                    ERR_PRINT("STRING.format Inner Array size != 2 ")
                }
            } else { //Array structure ["RobotGuy","Logis","rookie"]
                Variant v_val = values_arr[i];
                String val = v_val.as<String>();

                if (val.left(1) == "\"" && val.right(val.length() - 1) == "\"") {
                    val = val.substr(1, val.length() - 2);
                }

                if (StringUtils::contains(placeholder,"_")) {
                    new_string = new_string.replace(String(placeholder).replace("_", i_as_str), val);
                } else {
                    new_string = new_string.replace_first(placeholder, val);
                }
            }
        }
    } else if (values.get_type() == Variant::DICTIONARY) {
        Dictionary d = values;
        List<Variant> keys;
        d.get_key_list(&keys);

        for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
            String key = E->get().as<String>();
            String val = d[E->get()].as<String>();

            if (key.left(1) == "\"" && key.right(key.length() - 1) == "\"") {
                key = key.substr(1, key.length() - 2);
            }

            if (val.left(1) == "\"" && val.right(val.length() - 1) == "\"") {
                val = val.substr(1, val.length() - 2);
            }

            new_string = new_string.replace(String(placeholder).replace("_", key), val);
        }
    } else {
        ERR_PRINT("Invalid type: use Array or Dictionary.")
    }

    return new_string;
}

String String::replace_first(const String &p_key, const String &p_with) const {

    int pos = find(p_key);
    String res=*this;
    if(pos<0)
        return res;
    return res.replace(pos,p_key.length(),p_with);
}
String String::replacen(const String &p_key, const String &p_with) const {


    QString new_string=*this;
    return new_string.replace(p_key,p_with,Qt::CaseInsensitive);
}

String String::left(int p_pos) const {
    return mid(0, p_pos);
}

String String::right(int p_pos) const {

    return mid(p_pos);
}

CharType String::ord_at(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, length(), 0)
    return operator[](p_idx);
}

String String::dedent() const {

    QString new_string;
    String indent;
    bool has_indent = false;
    bool has_text = false;
    int line_start = 0;
    int indent_stop = -1;

    for (int i = 0; i < length(); i++) {

        CharType c = operator[](i);
        if (c == '\n') {
            if (has_text)
                new_string += substr(indent_stop, i - indent_stop);
            new_string += "\n";
            has_text = false;
            line_start = i + 1;
            indent_stop = -1;
        } else if (!has_text) {
            if (c > 32) {
                has_text = true;
                if (!has_indent) {
                    has_indent = true;
                    indent = substr(line_start, i - line_start);
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
        new_string += substr(indent_stop, length() - indent_stop);

    return String(new_string);
}

String String::strip_edges(bool left, bool right) const {

    int len = length();
    int beg = 0, end = len;

    if (left) {
        for (int i = 0; i < len; i++) {

            if (operator[](i) <= 32)
                beg++;
            else
                break;
        }
    }

    if (right) {
        for (int i = (int)(len - 1); i >= 0; i--) {

            if (operator[](i) <= 32)
                end--;
            else
                break;
        }
    }

    if (beg == 0 && end == len)
        return *this;

    return substr(beg, end - beg);
}

String String::strip_escapes() const {

    QString new_string;
    for (int i = 0; i < length(); i++) {

        // Escape characters on first page of the ASCII table, before 32 (Space).
        if (operator[](i) < 32)
            continue;
        new_string += operator[](i);
    }

    return String(new_string);
}

String String::lstrip(const String &p_chars) const {

    int len = length();
    int beg;

    for (beg = 0; beg < len; beg++) {

        if (p_chars.find_char(get(beg)) == -1)
            break;
    }

    if (beg == 0)
        return *this;

    return substr(beg, len - beg);
}

String String::rstrip(const String &p_chars) const {

    int len = length();
    int end;

    for (end = len - 1; end >= 0; end--) {

        if (p_chars.find_char(get(end)) == -1)
            break;
    }

    if (end == len - 1)
        return *this;

    return substr(0, end + 1);
}

String PathUtils::simplify_path(const String &str) {

    QString s = str;
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

    s = s.replace("\\", "/");
    while (true) { // in case of using 2 or more slash
        String compare = s.replace("//", "/");
        if (s == compare)
            break;
        else
            s = compare;
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

    s = "";

    for (int i = 0; i < dirs.size(); i++) {

        if (i > 0)
            s += "/";
        s += dirs[i];
    }

    return drive + String(s);
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
    static const char *prefix[] = { " Bytes", " KB", " MB", " GB", " TB", " PB", " EB", "" };
    int prefix_idx = 0;

    while (p_size > (_div * 1024) && prefix[prefix_idx][0]) {
        _div *= 1024;
        prefix_idx++;
    }

    int digits = prefix_idx > 0 ? _humanize_digits(p_size / _div) : 0;
    double divisor = prefix_idx > 0 ? _div : 1;

    return StringUtils::num(p_size / divisor).pad_decimals(digits) + prefix[prefix_idx];
}
bool PathUtils::is_abs_path(const String &str) {

    if (str.length() > 1)
        return (str[0] == '/' || str[0] == '\\' || str.contains(":/") || str.contains(":\\"));
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

//kind of poor should be rewritten properly

String StringUtils::word_wrap(const String &str,int p_chars_per_line) {

    int from = 0;
    int last_space = 0;
    QString ret;
    for (int i = 0; i < str.length(); i++) {
        if (i - from >= p_chars_per_line) {
            if (last_space == -1) {
                ret += str.mid(from, i - from + 1) + "\n";
            } else {
                ret += str.mid(from, last_space - from) + "\n";
                i = last_space; //rewind
            }
            from = i + 1;
            last_space = -1;
        } else if (str[i] == ' ' || str[i] == '\t') {
            last_space = i;
        } else if (str[i] == '\n') {
            ret += str.mid(from, i - from) + "\n";
            from = i + 1;
            last_space = -1;
        }
    }

    if (from < str.length()) {
        ret += str.substr(from);
    }

    return String(ret);
}

String StringUtils::http_escape(const String &s) {
    const CharString temp = s.toUtf8();
    QString res;
    for (int i = 0; i < temp.length(); ++i) {
        char ord = temp[i];
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
        if (str.ord_at(i) == '%' && i + 2 < str.length()) {
            CharType ord1 = str.ord_at(i + 1);
            if ((ord1 >= '0' && ord1 <= '9') || (ord1 >= 'A' && ord1 <= 'Z')) {
                CharType ord2 = str.ord_at(i + 2);
                if ((ord2 >= '0' && ord2 <= '9') || (ord2 >= 'A' && ord2 <= 'Z')) {
                    char bytes[3] = { (char)ord1.toLatin1(), (char)ord2.toLatin1(), 0 };
                    res += (char)strtol(bytes, nullptr, 16);
                    i += 2;
                }
            } else {
                res += str.ord_at(i);
            }
        } else {
            res += str.ord_at(i);
        }
    }
    return String::utf8(String(res).ascii());
}

String StringUtils::c_unescape(const String &str) {

    String escaped = str;
    escaped = escaped.replace("\\a", "\a");
    escaped = escaped.replace("\\b", "\b");
    escaped = escaped.replace("\\f", "\f");
    escaped = escaped.replace("\\n", "\n");
    escaped = escaped.replace("\\r", "\r");
    escaped = escaped.replace("\\t", "\t");
    escaped = escaped.replace("\\v", "\v");
    escaped = escaped.replace("\\'", "\'");
    escaped = escaped.replace("\\\"", "\"");
    escaped = escaped.replace("\\?", "\?");
    escaped = escaped.replace("\\\\", "\\");

    return escaped;
}

String StringUtils::c_escape(const String &e) {

    String escaped = e;
    escaped = escaped.replace("\\", "\\\\");
    escaped = escaped.replace("\a", "\\a");
    escaped = escaped.replace("\b", "\\b");
    escaped = escaped.replace("\f", "\\f");
    escaped = escaped.replace("\n", "\\n");
    escaped = escaped.replace("\r", "\\r");
    escaped = escaped.replace("\t", "\\t");
    escaped = escaped.replace("\v", "\\v");
    escaped = escaped.replace("\'", "\\'");
    escaped = escaped.replace("\?", "\\?");
    escaped = escaped.replace("\"", "\\\"");

    return escaped;
}

String StringUtils::c_escape_multiline(const String &str) {

    String escaped = str;
    escaped = escaped.replace("\\", "\\\\");
    escaped = escaped.replace("\"", "\\\"");

    return escaped;
}

String StringUtils::json_escape(const String &str) {

    String escaped = str;
    escaped = escaped.replace("\\", "\\\\");
    escaped = escaped.replace("\b", "\\b");
    escaped = escaped.replace("\f", "\\f");
    escaped = escaped.replace("\n", "\\n");
    escaped = escaped.replace("\r", "\\r");
    escaped = escaped.replace("\t", "\\t");
    escaped = escaped.replace("\v", "\\v");
    escaped = escaped.replace("\"", "\\\"");

    return escaped;
}

String StringUtils::xml_escape(const String &arg,bool p_escape_quotes)  {

    String str = arg;
    str = str.replace("&", "&amp;");
    str = str.replace("<", "&lt;");
    str = str.replace(">", "&gt;");
    if (p_escape_quotes) {
        str = str.replace("'", "&apos;");
        str = str.replace("\"", "&quot;");
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
    int len = _xml_unescape(arg.constData(), l, nullptr);
    if (len == 0)
        return String();
    str.resize(len + 1);
    _xml_unescape(arg.constData(), l, str.data());
    str[len] = 0;
    return str;
}

String String::pad_decimals(int p_digits) const {

    String s = *this;
    int c = s.indexOf(".");

    if (c == -1) {
        if (p_digits <= 0) {
            return s;
        }
        s += ".";
        c = s.length() - 1;
    } else {
        if (p_digits <= 0) {
            return s.mid(0, c);
        }
    }

    if (s.length() - (c + 1) > p_digits) {
        s = s.mid(0, c + p_digits + 1);
    } else {
        while (s.length() - (c + 1) < p_digits) {
            s += "0";
        }
    }
    return s;
}

String String::pad_zeros(int p_digits) const {

    String s = *this;
    int end = s.find(".");

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

String String::trim_prefix(const String &p_prefix) const {

    String s = *this;
    if (s.begins_with(p_prefix)) {
        return s.substr(p_prefix.length(), s.length() - p_prefix.length());
    }
    return s;
}

String String::trim_suffix(const String &p_suffix) const {

    String s = *this;
    if (s.ends_with(p_suffix)) {
        return s.substr(0, s.length() - p_suffix.length());
    }
    return s;
}

bool StringUtils::is_valid_integer(const String &str) {

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

bool StringUtils::is_valid_hex_number(const String &str,bool p_with_prefix) {

    int len = str.length();

    if (len == 0)
        return false;

    int from = 0;
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
    if (str.front() == '+' || str.front() == '-') {
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

    String src = PathUtils::get_base_dir(String(QString(base).replace("\\", "/")));
    String dst = PathUtils::get_base_dir(String(p_path.replace("\\", "/")));
    String rel = PathUtils::path_to(src,dst);
    if (rel == dst) // failed
        return p_path;

    return rel + PathUtils::get_file(p_path);
}

String PathUtils::path_to(const String &str,String p_path) {

    QString src = QString(str).replace("\\", "/");
    QString dst = p_path.replace("\\", "/");
    if (!src.endsWith("/"))
        src += "/";
    if (!dst.endsWith("/"))
        dst += "/";

    String base;

    if (src.startsWith("res://") && dst.startsWith("res://")) {

        base = "res:/";
        src = src.replace("res://", "/");
        dst = dst.replace("res://", "/");

    } else if (src.startsWith("user://") && dst.startsWith("user://")) {

        base = "user:/";
        src = src.replace("user://", "/");
        dst = dst.replace("user://", "/");

    } else if (src.startsWith("/") && dst.startsWith("/")) {

        //nothing
    } else {
        //dos style
        String src_begin = String(src).get_slicec('/', 0);
        String dst_begin = String(dst).get_slicec('/', 0);

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

    String stripped = str.strip_edges();
    if (str != stripped) {
        return false;
    }

    if (stripped == String()) {
        return false;
    }
    //TODO: SEGS: convert this chain of string scans to something saner.
    return !(str.contains(":") || str.contains('/')|| str.contains('\\')|| str.contains('?')|| str.contains('*')|| str.contains('\"')|| str.contains('|')|| str.contains('%')|| str.contains('<')|| str.contains('>'));
}

bool StringUtils::is_valid_ip_address(const String &str) {

    if (str.find(":") >= 0) {

        Vector<String> ip = StringUtils::split(str,":");
        for (int i = 0; i < ip.size(); i++) {

            String n = ip[i];
            if (n.empty())
                continue;
            if (StringUtils::is_valid_hex_number(n,false)) {
                int nint = n.hex_to_int(false);
                if (nint < 0 || nint > 0xffff)
                    return false;
                continue;
            }
            if (!StringUtils::is_valid_ip_address(n))
                return false;
        }

    } else {
        Vector<String> ip = StringUtils::split(str,".");
        if (ip.size() != 4)
            return false;
        for (int i = 0; i < ip.size(); i++) {

            String n = ip[i];
            if (!StringUtils::is_valid_integer(n))
                return false;
            int val = n.to_int();
            if (val < 0 || val > 255)
                return false;
        }
    }

    return true;
}

bool PathUtils::is_resource_file(const String &str) {

    return str.begins_with("res://") && !str.contains("::");
}

bool PathUtils::is_rel_path(const String &str) {

    return !is_abs_path(str);
}

String PathUtils::get_base_dir(const String &path) {

    int basepos = path.find("://");
    String rs;
    String base;
    if (basepos != -1) {
        int end = basepos + 3;
        rs = path.substr(end);
        base = path.substr(0, end);
    } else {
        if (path.startsWith('/')) {
            rs = path.substr(1);
            base = "/";
        } else {

            rs = path;
        }
    }

    int sep = std::max(rs.find_last("/"), rs.find_last("\\"));
    if (sep == -1)
        return base;

    return base + rs.substr(0, sep);
}

String PathUtils::get_file(const String &path) {

    int sep = MAX(path.lastIndexOf('/'), path.lastIndexOf('\\'));
    if (sep == -1)
        return path;

    return path.mid(sep + 1);
}

String PathUtils::get_extension(const String &path) {

    int pos = path.lastIndexOf(".");
    if (pos < 0 || pos < MAX(path.lastIndexOf("/"), path.lastIndexOf("\\")))
        return "";

    return path.mid(pos + 1);
}

String PathUtils::plus_file(const String &bp,const String &p_file) {
    if (bp.isEmpty())
        return p_file;
    if (bp.back() == '/' || (p_file.size() > 0 && bp.front() == '/'))
        return bp + p_file;
    return bp + "/" + p_file;
}

String StringUtils::percent_encode(const String &str) {

    CharString cs = StringUtils::to_utf8(str);
    String encoded;
    for (int i = 0; i < cs.length(); i++) {
        uint8_t c = cs[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '~' || c == '.') {

            char p[2] = { (char)c, 0 };
            encoded += p;
        } else {
            char p[4] = { '%', 0, 0, 0 };
            static const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

            p[1] = hex[c >> 4];
            p[2] = hex[c & 0xF];
            encoded += p;
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

    int pos = path.lastIndexOf('.');
    if (pos < 0 || pos < MAX(path.lastIndexOf("/"), path.lastIndexOf("\\")))
        return path;

    return path.mid(0, pos);
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
String String::rpad(int min_length, CharType character) const {
    String s = *this;
    int padding = min_length - s.length();
    if (padding > 0) {
        for (int i = 0; i < padding; i++)
            s = s + character;
    }

    return s;
}
// Left-pad with a character.
String String::lpad(int min_length, CharType character) const {
    String s = *this;
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
String String::sprintf(const Array &values, bool *error) const {
    String formatted;
    CharType *self = (CharType *)constData();
    bool in_format = false;
    int value_index = 0;
    int min_chars = 0;
    int min_decimals = 0;
    bool in_decimals = false;
    bool pad_with_zeroes = false;
    bool left_justified = false;
    bool show_sign = false;

    *error = true;

    for (; self->isNull(); self++) {
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
                        return "not enough arguments for format string";
                    }

                    if (!values[value_index].is_num()) {
                        return "a number is required";
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
                        str = str.rpad(min_chars, pad_char);
                    } else {
                        str = str.lpad(min_chars, pad_char);
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
                        return "not enough arguments for format string";
                    }

                    if (!values[value_index].is_num()) {
                        return "a number is required";
                    }

                    double value = values[value_index];
                    String str = StringUtils::num(value, min_decimals);

                    // Pad decimals out.
                    str = str.pad_decimals(min_decimals);

                    // Show sign
                    if (show_sign && str.left(1) != "-") {
                        str = str.insert(0, "+");
                    }

                    // Padding
                    if (left_justified) {
                        str = str.rpad(min_chars);
                    } else {
                        str = str.lpad(min_chars);
                    }

                    formatted += str;
                    ++value_index;
                    in_format = false;

                    break;
                }
                case 's': { // String
                    if (value_index >= values.size()) {
                        return "not enough arguments for format string";
                    }

                    String str = values[value_index].as<String>();
                    // Padding.
                    if (left_justified) {
                        str = str.rpad(min_chars);
                    } else {
                        str = str.lpad(min_chars);
                    }

                    formatted += str;
                    ++value_index;
                    in_format = false;
                    break;
                }
                case 'c': {
                    if (value_index >= values.size()) {
                        return "not enough arguments for format string";
                    }

                    // Convert to character.
                    String str;
                    if (values[value_index].is_num()) {
                        int value = values[value_index];
                        if (value < 0) {
                            return "unsigned byte integer is lower than maximum";
                        } else if (value > 255) {
                            return "unsigned byte integer is greater than maximum";
                        }
                        str = values[value_index].as<QChar>();
                    } else if (values[value_index].get_type() == Variant::STRING) {
                        str = values[value_index].as<String>();
                        if (str.length() != 1) {
                            return "%c requires number or single-character string";
                        }
                    } else {
                        return "%c requires number or single-character string";
                    }

                    // Padding.
                    if (left_justified) {
                        str = str.rpad(min_chars);
                    } else {
                        str = str.lpad(min_chars);
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
                        return "too many decimal points in format";
                    }
                    in_decimals = true;
                    min_decimals = 0; // We want to add the value manually.
                    break;
                }

                case '*': { // Dynamic width, based on value.
                    if (value_index >= values.size()) {
                        return "not enough arguments for format string";
                    }

                    if (!values[value_index].is_num()) {
                        return "* wants number";
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
                    return "unsupported format character";
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
        return "incomplete format";
    }

    if (value_index != values.size()) {
        return "not all arguments converted during string formatting";
    }

    *error = false;
    return formatted;
}

String String::quote(CharType character) const {
    return character + *this + character;
}

String String::unquote() const {
    if (!StringUtils::is_quoted(*this)) {
        return *this;
    }

    return substr(1, length() - 2);
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
        if (rtr == String() || rtr == p_text) {
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
        return lhs.compare(rhs,Qt::CaseSensitive);
    } else if(case_sensitive==CaseInsensitive)
    {
        return lhs.compare(rhs,Qt::CaseInsensitive);
    }
    QCollator col;
    col.setNumericMode(true);
    return col.compare(lhs,rhs);
}

bool StringUtils::contains(const char *heystack, const char *needle)
{
    std::string_view sv1(heystack);
    std::string_view nd1(needle);
    return sv1.find(nd1)!=std::string_view::npos;
}
