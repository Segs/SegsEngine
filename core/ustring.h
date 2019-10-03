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

class GODOT_EXPORT String {

    static const CharType _null;
    void copy_from_unchecked(const CharType *p_char, const int p_length);
public:
    static const String null_val;
    QString m_str;
    void clear() { m_str.clear(); }
    CharType *data() {
        return m_str.data();
    }
    const CharType *cdata() const {
        return m_str.constData();
    }
    CharType front() const { return m_str.front(); }
    CharType back() const { return m_str.back(); }
    //void remove(int p_index) { this->re.remove(p_index); }

    const CharType operator[](int p_index) const {
        if(p_index==size())
            return CharType(); // Godot logic assumes accessing str[length] will return null char :|
        return m_str[p_index];
    }
    _FORCE_INLINE_ void set(int p_index, CharType p_elem) {
        m_str[p_index] = p_elem;
    }

    /* Compatibility Operators */

    bool operator==(const StrRange &p_str_range) const;

    static String asprintf(const char *format, ...) Q_ATTRIBUTE_FORMAT_PRINTF(1, 2);

    _FORCE_INLINE_ bool empty() const { return m_str.isEmpty(); }
    [[nodiscard]] int element_count() const { return m_str.size(); }
    /*[[deprecated]] */ int size() const { return m_str.size(); }
    /*[[deprecated]] */ int length() const { return m_str.length(); }
    /**
     * The constructors must not depend on other overloads
     */
    bool operator!=(const char *v) const { return m_str!=v; }
    bool operator==(const String &v) const { return m_str==v.m_str; }
    bool operator!=(const String &v) const { return m_str!=v.m_str; }
    bool operator<(const String &b) const{ return m_str<b.m_str;}
    bool operator<=(const String &b) const { return m_str<=b.m_str;}
    bool operator==(const char *v) const { return m_str==v; }

    String &operator+=(const String &rhs) {
        m_str += rhs.m_str;
        return *this;
    }
    String &operator+=(StringView rhs) {
        m_str.append(rhs.data(),rhs.size());
        return *this;
    }
    String &operator+=(CharType rhs) {
        m_str += rhs;
        return *this;
    }
    String operator+(const String &rhs) const {
        return m_str + rhs.m_str;
    }
    String operator+(const char *rhs) const {
        return m_str + QString(rhs);
    }
    String operator+(const CharType rhs) const {
        return m_str + QString(rhs);
    }

    String() = default;
    String(std::nullptr_t ) : m_str() {}
    String(const StrRange &p_range);
    String(QString &&o) : m_str(std::move(o)) {}
    String(const QString &o) : m_str(o) {}
    String(const String &o) = default;
    String(String &&o) = default;
    explicit String(CharType x) : m_str(x) {}
    //TODO: mark const char * String constructor as explicit to catch all manner of dynamic allocations.
    String(const char *s) : m_str(s) {}
    explicit String(const CharType *s,int size=-1) { m_str = QString(s,size); }
    String &operator=(const String &) = default;
    String &operator=(String &&) = default;
    String &operator=(const char *s) { m_str = s; return *this; }
    String(StringView v) : m_str(v.toString()) {}

};
inline String operator+(const char *a,const String &b) { return String(a)+b;}
inline String operator+(CharType a,const String &b) { return String(a+b.m_str);}
inline bool operator==(const char *a,const String &b) { return a==b.m_str;}

namespace StringUtils
{
enum Compare {
    CaseSensitive=0,
    CaseInsensitive,
    CaseNatural
};
//! length in codepoints, not bytes
[[nodiscard]] inline int char_length(const String &str) {
    return str.m_str.size();
}

[[nodiscard]] GODOT_EXPORT double to_double(const String &str);
[[nodiscard]] GODOT_EXPORT double to_double(const char *p_str);
GODOT_EXPORT double to_double(const CharType *p_str, const CharType **r_end = nullptr);
[[nodiscard]] GODOT_EXPORT float to_float(const String &str);
[[nodiscard]] GODOT_EXPORT int hex_to_int(const String &str,bool p_with_prefix = true);
[[nodiscard]] GODOT_EXPORT inline int to_int(const String &str) { return str.m_str.toInt(); }
[[nodiscard]] GODOT_EXPORT int to_int(const char *p_str, int p_len = -1);
[[nodiscard]] GODOT_EXPORT int64_t to_int(const CharType *p_str, int p_len = -1);
[[nodiscard]] GODOT_EXPORT int64_t to_int64(const String &str);

[[nodiscard]] GODOT_EXPORT String format(const String &str,const Variant &values);

[[nodiscard]] GODOT_EXPORT String num(double p_num, int p_decimals = -1);
[[nodiscard]] GODOT_EXPORT String num_scientific(double p_num);
[[nodiscard]] GODOT_EXPORT String num_real(double p_num);
[[nodiscard]] GODOT_EXPORT String num_int64(int64_t p_num, int base = 10, bool capitalize_hex = false);
[[nodiscard]] GODOT_EXPORT String num_uint64(uint64_t p_num, int base = 10, bool capitalize_hex = false);
[[nodiscard]] GODOT_EXPORT String md5(const uint8_t *p_md5);
[[nodiscard]] GODOT_EXPORT String hex_encode_buffer(const uint8_t *p_buffer, int p_len);

[[nodiscard]] GODOT_EXPORT float similarity(const String &lhs,const String &p_string);
[[nodiscard]] GODOT_EXPORT Vector<String> bigrams(const String &str);
[[nodiscard]] GODOT_EXPORT int compare(const String &lhs,const String &rhs,Compare case_sensitive=CaseSensitive);
[[nodiscard]] GODOT_EXPORT String percent_encode(const String &src) ;
[[nodiscard]] GODOT_EXPORT String percent_decode(const String &src);

[[nodiscard]] GODOT_EXPORT String md5_text(const String &str);
[[nodiscard]] GODOT_EXPORT String sha1_text(const String &str);
[[nodiscard]] GODOT_EXPORT String sha256_text(const String &str);
[[nodiscard]] GODOT_EXPORT Vector<uint8_t> md5_buffer(const String &str);
[[nodiscard]] GODOT_EXPORT Vector<uint8_t> sha1_buffer(const String &str);
[[nodiscard]] GODOT_EXPORT Vector<uint8_t> sha256_buffer(const String &str);

[[nodiscard]] GODOT_EXPORT CharType char_uppercase(CharType p_char);
[[nodiscard]] GODOT_EXPORT CharType char_lowercase(CharType p_char);

[[nodiscard]] GODOT_EXPORT String to_upper(const String &s);
[[nodiscard]] GODOT_EXPORT String to_lower(const String &s);

[[nodiscard]] GODOT_EXPORT String xml_escape(const String &src,bool p_escape_quotes = false);
[[nodiscard]] GODOT_EXPORT String xml_unescape(const String &src);
[[nodiscard]] GODOT_EXPORT String http_escape(const String &src);
[[nodiscard]] GODOT_EXPORT String http_unescape(const String &src);
[[nodiscard]] GODOT_EXPORT String c_escape(const String &src);
[[nodiscard]] GODOT_EXPORT String c_escape_multiline(const String &src);
[[nodiscard]] GODOT_EXPORT String c_unescape(const String &src);
[[nodiscard]] GODOT_EXPORT String json_escape(const String &src);
[[nodiscard]] GODOT_EXPORT String word_wrap(const String &src,int p_chars_per_line);

[[nodiscard]] GODOT_EXPORT bool is_subsequence_of(const String &str,const String &p_string, Compare mode=CaseSensitive);
[[nodiscard]] inline bool is_subsequence_ofi(const String &str,const String &p_string) {
    return is_subsequence_of(str,p_string, CaseInsensitive);
}
[[nodiscard]] GODOT_EXPORT bool is_quoted(const String &str);
[[nodiscard]] GODOT_EXPORT bool is_numeric(const String &str);
[[nodiscard]] GODOT_EXPORT bool is_valid_identifier(const String &src);
[[nodiscard]] GODOT_EXPORT bool is_valid_integer(const String &src);
[[nodiscard]] GODOT_EXPORT bool is_valid_float(const String &src);
[[nodiscard]] GODOT_EXPORT bool is_valid_hex_number(const String &src,bool p_with_prefix);
[[nodiscard]] GODOT_EXPORT bool is_valid_html_color(const String &src);
[[nodiscard]] GODOT_EXPORT bool is_valid_ip_address(const String &src);
[[nodiscard]] GODOT_EXPORT bool is_valid_filename(const String &src);
[[nodiscard]] GODOT_EXPORT bool contains(const char *heystack, const char *needle, Compare mode=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool contains(const String &heystack,CharType c,Compare mode=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool contains(const String &heystack,const String &needle,Compare mode=CaseSensitive);
[[nodiscard]] GODOT_EXPORT Vector<String> split(const String &str,CharType p_splitter, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT Vector<String> split(const String &str,const String &p_splitter, bool p_allow_empty = true, int p_maxsplit = 0);
[[nodiscard]] GODOT_EXPORT Vector<String> rsplit(const String &str,const String &p_splitter, bool p_allow_empty = true, int p_maxsplit = 0);
[[nodiscard]] GODOT_EXPORT Vector<String> split_spaces(const String &str);
[[nodiscard]] GODOT_EXPORT Vector<float> split_floats(const String &str,const String &p_splitter, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT Vector<float> split_floats_mk(const String &str,const Vector<String> &p_splitters, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT Vector<int> split_ints(const String &str,const String &p_splitter, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT Vector<int> split_ints_mk(const String &str,const Vector<String> &p_splitters, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT String join(const String &str, const Vector<String> &parts);
[[nodiscard]] GODOT_EXPORT String repeat(const String &str,int p_count);

[[nodiscard]] GODOT_EXPORT String quote(const String &str,CharType character = '\"');
[[nodiscard]] GODOT_EXPORT String unquote(const String &str);

static_assert (sizeof(CharType)==sizeof(uint16_t), "Hash functions assume CharType's underlying type is uint16_t");
// String hashing helpers
static inline uint32_t hash(const CharType *p_cstr, int p_len) {
    return hash_djb2_buffer((const uint16_t *)p_cstr,p_len);
}
static inline uint32_t hash(const CharType *p_cstr) {
    return hash_djb2((const uint16_t *)p_cstr);
}
static inline uint32_t hash(const char *p_cstr, int p_len) {
    return hash_djb2_buffer((const uint8_t *)p_cstr,p_len);
}
static inline uint32_t hash(const char *p_cstr) {
    return hash_djb2(p_cstr);
}
/* hash the string */
static inline uint32_t hash(const String &s) {
    return hash_djb2_buffer((const uint16_t *)s.cdata(),s.length());
}
/* hash the string */
static inline uint64_t hash64(const String &s) {
    return hash_djb2_buffer64((const uint16_t *)s.cdata(),s.length());
}

[[nodiscard]] GODOT_EXPORT bool begins_with(const String &str,CharType ch);
[[nodiscard]] GODOT_EXPORT bool begins_with(const String &str,const String &p_string);
[[nodiscard]] GODOT_EXPORT bool begins_with(const String &str,const char *p_string);
[[nodiscard]] GODOT_EXPORT bool ends_with(const String &str,const String &p_string);
[[nodiscard]] GODOT_EXPORT bool ends_with(const String &s, CharType ch);

[[nodiscard]] GODOT_EXPORT int64_t hex_to_int64(const String &str,bool p_with_prefix = true);
[[nodiscard]] GODOT_EXPORT int64_t bin_to_int64(const String &str,bool p_with_prefix = true);

[[nodiscard]] GODOT_EXPORT String capitalize(const String &str);
[[nodiscard]] GODOT_EXPORT String camelcase_to_underscore(const String &str,bool lowercase = true);

[[nodiscard]] GODOT_EXPORT int count(const String &heystack,const String &p_string, int p_from = 0, int p_to = 0);
[[nodiscard]] GODOT_EXPORT int countn(const String &heystack,const String &p_string, int p_from = 0, int p_to = 0);

[[nodiscard]] GODOT_EXPORT int get_slice_count(const String &str, const String &p_splitter);
[[nodiscard]] GODOT_EXPORT int get_slice_count(const String &str,CharType p_splitter);

[[nodiscard]] GODOT_EXPORT String get_slice(const String &str, const String &p_splitter, int p_slice);
[[nodiscard]] GODOT_EXPORT String get_slice(const String &str,CharType p_splitter, int p_slice);

[[nodiscard]] GODOT_EXPORT String left(const String &str,int p_pos);
[[nodiscard]] GODOT_EXPORT String right(const String &str,int p_pos);
[[nodiscard]] GODOT_EXPORT String dedent(const String &str);
[[nodiscard]] GODOT_EXPORT String strip_edges(const String &str,bool left = true, bool right = true);
[[nodiscard]] GODOT_EXPORT String strip_escapes(const String &str);
[[nodiscard]] GODOT_EXPORT String lstrip(const String &str,const String &p_chars);
[[nodiscard]] GODOT_EXPORT String rstrip(const String &str,const String &p_chars);
[[nodiscard]] GODOT_EXPORT String substr(const String &str,int p_from, int p_chars = -1);

[[nodiscard]] GODOT_EXPORT int find(const String &str,const String &p_str, int p_from = 0); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int find(const String &str,const char *p_str, int p_from = 0); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int find_char(const String &str,CharType p_char, int p_from = 0); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int find_last(const String &str,const String &p_str); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int find_last(const String &str,CharType p_ch); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int findn(const String &str,const String &p_str, int p_from = 0); ///< return <0 if failed, case insensitive
[[nodiscard]] GODOT_EXPORT int rfind(const String &str,const String &p_str, int p_from = -1); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int rfindn(const String &str,const String &p_str, int p_from = -1); ///< return <0 if failed, case insensitive
[[nodiscard]] GODOT_EXPORT int findmk(const String &str,const Vector<String> &p_keys, int p_from = 0, int *r_key = nullptr); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT bool match(const String &str,const String &p_wildcard,Compare sensitivity=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool matchn(const String &str,const String &p_wildcard);
/* complex helpers */

[[nodiscard]] CharType ord_at(const String &str,int p_idx);

void erase(String &str,int p_pos, int p_chars);

[[nodiscard]] GODOT_EXPORT CharString ascii(const String &str,bool p_allow_extended = false);
[[nodiscard]] GODOT_EXPORT CharString utf8(const String &str);
GODOT_EXPORT bool parse_utf8(String &str,const char *p_utf8, int p_len = -1); //return true on error
[[nodiscard]] GODOT_EXPORT String from_utf8(const char *p_utf8, int p_len = -1);
[[nodiscard]] GODOT_EXPORT String from_wchar(const wchar_t *p_utf8, int p_len = -1);
[[nodiscard]] inline CharString to_utf8(const String &s) {
    return s.m_str.toUtf8();
}
[[nodiscard]] inline WString to_wstring(const String &s) {
    return s.m_str.toStdWString();
}
[[nodiscard]] GODOT_EXPORT String replace(const String &str,const String &p_key, const String &p_with);
[[nodiscard]] GODOT_EXPORT String replace(const String &str,CharType p_key, CharType p_with);
[[nodiscard]] GODOT_EXPORT String replace(const String &s,int i,int len, const String &p_after);
[[nodiscard]] GODOT_EXPORT String replace(const String &str,const char *p_key, const char *p_with);

namespace Inplace {
    GODOT_EXPORT void replace(String &str,const String &p_key, const String &p_with);
    GODOT_EXPORT void replace(String &str,CharType p_key, CharType p_with);
    GODOT_EXPORT void replace(String &s,int i,int len, const String &p_after);
    GODOT_EXPORT void replace(String &str,const char *p_key, const char *p_with);
}
[[nodiscard]] GODOT_EXPORT String replace_first(const String &str,const String &p_key, const String &p_with);
[[nodiscard]] GODOT_EXPORT String replacen(const String &str,const String &p_key, const String &p_with);
[[nodiscard]] GODOT_EXPORT String insert(const String &str,int p_at_pos, const String &p_string);
[[nodiscard]] GODOT_EXPORT String pad_decimals(const String &str,int p_digits);
[[nodiscard]] GODOT_EXPORT String pad_zeros(const String &str,int p_digits);
[[nodiscard]] GODOT_EXPORT String trim_prefix(const String &str,const String &p_prefix);
[[nodiscard]] GODOT_EXPORT String trim_suffix(const String &str,const String &p_suffix);
[[nodiscard]] GODOT_EXPORT String lpad(const String &str,int min_length, CharType character = ' ');
[[nodiscard]] GODOT_EXPORT String rpad(const String &str,int min_length, CharType character = ' ');
[[nodiscard]] GODOT_EXPORT String sprintf(const String &str,const Array &values, bool *error);

} // end of StringUtils namespace

namespace PathUtils
{
GODOT_EXPORT String get_extension(const String &p);
GODOT_EXPORT String get_basename(const String &p);
[[nodiscard]] GODOT_EXPORT String plus_file(const String &p,const String &p_file);
//! @note for now it just replaces \\ with /
[[nodiscard]] inline String from_native_path(const String &p) {
    return StringUtils::replace(p,'\\', '/');
}
[[nodiscard]] inline String to_win_path(const String &v)
{
    return StringUtils::replace(v,"/", "\\");
}
// path functions
[[nodiscard]] GODOT_EXPORT bool is_abs_path(const String &p);
[[nodiscard]] GODOT_EXPORT bool is_rel_path(const String &p);
[[nodiscard]] GODOT_EXPORT bool is_resource_file(const String &p);
[[nodiscard]] GODOT_EXPORT String path_to(const String &p,String p_path);
[[nodiscard]] GODOT_EXPORT String path_to_file(const String &p,String p_path);
[[nodiscard]] GODOT_EXPORT String get_base_dir(const String &p);
[[nodiscard]] GODOT_EXPORT String get_file(const String &p);
[[nodiscard]] GODOT_EXPORT String humanize_size(size_t p_size);
[[nodiscard]] GODOT_EXPORT String simplify_path(const String &p);
[[nodiscard]] GODOT_EXPORT String trim_trailing_slash(const String &path);
[[nodiscard]] GODOT_EXPORT bool is_internal_path(const String &path);
} // end o PathUtils namespace


GODOT_EXPORT String itos(int64_t p_val);
GODOT_EXPORT String rtos(double p_val);
GODOT_EXPORT String rtoss(double p_val); // scientific version

struct NoCaseComparator {

    bool operator()(const String &p_a, const String &p_b) const noexcept {

        return StringUtils::compare(p_a,p_b,StringUtils::CaseInsensitive) < 0;
    }
};

struct NaturalNoCaseComparator {

    bool operator()(const String &p_a, const String &p_b) const {

        return StringUtils::compare(p_a,p_b,StringUtils::CaseNatural) < 0;
    }
};

/* end of namespace */

//tool translate
#ifdef TOOLS_ENABLED

//gets parsed
String TTR(const String &);
//use for C strings
#define TTRC(m_value) (m_value)
//use to avoid parsing (for use later with C strings)
#define TTRGET(m_value) TTR(m_value)

#else

#define TTR(m_value) (String())
#define TTRC(m_value) (m_value)
#define TTRGET(m_value) (m_value)

#endif

//tool or regular translate
String RTR(const String &);

bool is_symbol(CharType c);
bool select_word(const String &p_s, int p_col, int &r_beg, int &r_end);

template<>
struct Hasher<String> {
    _FORCE_INLINE_ uint32_t operator()(const String &p_string) const { return StringUtils::hash(p_string); }
};
