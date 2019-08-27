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


#include "core/array.h"

// We don't include cowdata_impl in hope the extern templates are enough :)
#include "core/cowdata.h"

#include "core/typedefs.h"
#include "core/vector.h"
#include "core/hashfuncs.h"

#include <QtCore/QString>

using CharString = QByteArray;
using CharProxy = QCharRef;


using CharType = QChar;

struct StrRange {

    const CharType *c_str;
    int len;

    StrRange(const CharType *p_c_str = nullptr, int p_len = 0) {
        c_str = p_c_str;
        len = p_len;
    }
};

class GODOT_EXPORT String : public QString {

    static const CharType _null;
    QString m_contents;
    void copy_from_unchecked(const CharType *p_char, const int p_length);
    int _count(const String &p_string, int p_from, int p_to, bool p_case_insensitive) const;

public:
    CharType *data() {
        return QString::data();
    }
    const CharType *cdata() const {
        return QString::constData();
    }

    //void remove(int p_index) { this->re.remove(p_index); }

    _FORCE_INLINE_ CharType get(int p_index) const { return m_contents[p_index]; }
    _FORCE_INLINE_ void set(int p_index, CharType p_elem) {
        m_contents[p_index] = p_elem;
    }

    /* Compatibility Operators */

    bool operator==(const StrRange &p_str_range) const;

    int naturalnocasecmp_to(const String &p_str) const;

    /* standard size stuff */

    _FORCE_INLINE_ int length() const {
        int s = m_contents.size();
        return s ? (s - 1) : 0; // length does not include zero
    }

    /* complex helpers */
    String substr(int p_from, int p_chars = -1) const;
    int find(const String &p_str, int p_from = 0) const; ///< return <0 if failed
    int find(const char *p_str, int p_from = 0) const; ///< return <0 if failed
    int find_char(const CharType &p_char, int p_from = 0) const; ///< return <0 if failed
    int find_last(const String &p_str) const; ///< return <0 if failed
    int findn(const String &p_str, int p_from = 0) const; ///< return <0 if failed, case insensitive
    int rfind(const String &p_str, int p_from = -1) const; ///< return <0 if failed
    int rfindn(const String &p_str, int p_from = -1) const; ///< return <0 if failed, case insensitive
    int findmk(const Vector<String> &p_keys, int p_from = 0, int *r_key = nullptr) const; ///< return <0 if failed
    bool match(const String &p_wildcard) const;
    bool matchn(const String &p_wildcard) const;
    bool begins_with(const String &p_string) const;
    bool begins_with(const char *p_string) const;
    bool ends_with(const String &p_string) const;

    Vector<String> bigrams() const;
    String format(const Variant &values, const char * placeholder = "{_}") const;
    String replace_first(const String &p_key, const String &p_with) const;
    String replacen(const String &p_key, const String &p_with) const;
    String insert(int p_at_pos, const String &p_string) const;
    String pad_decimals(int p_digits) const;
    String pad_zeros(int p_digits) const;
    String trim_prefix(const String &p_prefix) const;
    String trim_suffix(const String &p_suffix) const;
    String lpad(int min_length, CharType character = ' ') const;
    String rpad(int min_length, CharType character = ' ') const;
    String sprintf(const Array &values, bool *error) const;
    String quote(CharType character = '\"') const;
    String unquote() const;
    double to_double() const;
    float to_float() const;
    int hex_to_int(bool p_with_prefix = true) const;
    int to_int() const { return m_contents.toInt(); }

    int64_t hex_to_int64(bool p_with_prefix = true) const;
    int64_t bin_to_int64(bool p_with_prefix = true) const;
    int64_t to_int64() const;
    String capitalize() const;
    String camelcase_to_underscore(bool lowercase = true) const;

    int get_slice_count(String p_splitter) const;
    String get_slice(String p_splitter, int p_slice) const;
    String get_slicec(CharType p_splitter, int p_slice) const;


    int count(const String &p_string, int p_from = 0, int p_to = 0) const;
    int countn(const String &p_string, int p_from = 0, int p_to = 0) const;

    String left(int p_pos) const;
    String right(int p_pos) const;
    String dedent() const;
    String strip_edges(bool left = true, bool right = true) const;
    String strip_escapes() const;
    String lstrip(const String &p_chars) const;
    String rstrip(const String &p_chars) const;

    CharType ord_at(int p_idx) const;

    void erase(int p_pos, int p_chars);

    CharString ascii(bool p_allow_extended = false) const;
    CharString utf8() const;
    bool parse_utf8(const char *p_utf8, int p_len = -1); //return true on error
    static String utf8(const char *p_utf8, int p_len = -1);

    _FORCE_INLINE_ bool empty() const { return m_contents.isEmpty(); }

//    String operator+(const String &o) const {
//        return String(m_contents+o.m_contents);
//    }
//    const String operator+=(const String &o) {
//        m_contents+=o.m_contents;
//        return *this;
//    }
//    CharType operator[](int idx) const { return m_contents[idx]; }
    /**
     * The constructors must not depend on other overloads
     */
    String(const StrRange &p_range);
    String(QString &&o) : m_contents(std::move(o)) {}
    String(const QString &o) : m_contents(o) {}
    String(const String &o) = default;
    String(String &&o) = default;
    String &operator=(const String &) = default;
    using QString::QString;
};

namespace PathUtils
{
String get_extension(const String &p);
String get_basename(const String &p);
[[nodiscard]] String plus_file(const String &p,const String &p_file);

// path functions
[[nodiscard]] bool is_abs_path(const String &p);
[[nodiscard]] bool is_rel_path(const String &p);
[[nodiscard]] bool is_resource_file(const String &p);
[[nodiscard]] String path_to(const String &p,String p_path);
[[nodiscard]] String path_to_file(const String &p,String p_path);
[[nodiscard]] String get_base_dir(const String &p);
[[nodiscard]] String get_file(const String &p);
[[nodiscard]] String humanize_size(size_t p_size);
[[nodiscard]] String simplify_path(const String &p);
}
namespace StringUtils
{
enum Compare {
    CaseSensitive=0,
    CaseInsensitive,
    CaseNatural
};
[[nodiscard]] inline CharString to_utf8(const String &s) { return s.toUtf8(); }
[[nodiscard]] String num(double p_num, int p_decimals = -1);
[[nodiscard]] String num_scientific(double p_num);
[[nodiscard]] String num_real(double p_num);
[[nodiscard]] String num_int64(int64_t p_num, int base = 10, bool capitalize_hex = false);
[[nodiscard]] String num_uint64(uint64_t p_num, int base = 10, bool capitalize_hex = false);
[[nodiscard]] String md5(const uint8_t *p_md5);
[[nodiscard]] String hex_encode_buffer(const uint8_t *p_buffer, int p_len);
[[nodiscard]] int to_int(const char *p_str, int p_len = -1);
[[nodiscard]] double to_double(const char *p_str);
double to_double(const CharType *p_str, const CharType **r_end = nullptr);
[[nodiscard]] int64_t to_int(const CharType *p_str, int p_len = -1);

[[nodiscard]] float similarity(const String &lhs,const String &p_string);
[[nodiscard]] Vector<String> bigrams(const String &str);
[[nodiscard]] int compare(const String &lhs,const String &rhs,Compare case_sensitive=CaseSensitive);
[[nodiscard]] String percent_encode(const String &src) ;
[[nodiscard]] String percent_decode(const String &src);

[[nodiscard]] String md5_text(const String &str);
[[nodiscard]] String sha1_text(const String &str);
[[nodiscard]] String sha256_text(const String &str);
[[nodiscard]] Vector<uint8_t> md5_buffer(const String &str);
[[nodiscard]] Vector<uint8_t> sha1_buffer(const String &str);
[[nodiscard]] Vector<uint8_t> sha256_buffer(const String &str);

[[nodiscard]] CharType char_uppercase(CharType p_char);
[[nodiscard]] CharType char_lowercase(CharType p_char);

[[nodiscard]] String to_upper(const String &s);
[[nodiscard]] String to_lower(const String &s);

[[nodiscard]] String xml_escape(const String &src,bool p_escape_quotes = false);
[[nodiscard]] String xml_unescape(const String &src);
[[nodiscard]] String http_escape(const String &src);
[[nodiscard]] String http_unescape(const String &src);
[[nodiscard]] String c_escape(const String &src);
[[nodiscard]] String c_escape_multiline(const String &src);
[[nodiscard]] String c_unescape(const String &src);
[[nodiscard]] String json_escape(const String &src);
[[nodiscard]] String word_wrap(const String &src,int p_chars_per_line);

[[nodiscard]] bool is_subsequence_of(const String &str,const String &p_string);
[[nodiscard]] bool is_subsequence_ofi(const String &str,const String &p_string);
[[nodiscard]] bool is_quoted(const String &str);
[[nodiscard]] bool is_numeric(const String &str);
[[nodiscard]] bool is_valid_identifier(const String &src);
[[nodiscard]] bool is_valid_integer(const String &src);
[[nodiscard]] bool is_valid_float(const String &src);
[[nodiscard]] bool is_valid_hex_number(const String &src,bool p_with_prefix);
[[nodiscard]] bool is_valid_html_color(const String &src);
[[nodiscard]] bool is_valid_ip_address(const String &src);
[[nodiscard]] bool is_valid_filename(const String &src);
[[nodiscard]] bool contains(const char *heystack,const char *needle);
Vector<String> split(const String &str,const String &p_splitter, bool p_allow_empty = true, int p_maxsplit = 0);
Vector<String> rsplit(const String &str,const String &p_splitter, bool p_allow_empty = true, int p_maxsplit = 0);
Vector<String> split_spaces(const String &str);
Vector<float> split_floats(const String &str,const String &p_splitter, bool p_allow_empty = true);
Vector<float> split_floats_mk(const String &str,const Vector<String> &p_splitters, bool p_allow_empty = true);
Vector<int> split_ints(const String &str,const String &p_splitter, bool p_allow_empty = true);
Vector<int> split_ints_mk(const String &str,const Vector<String> &p_splitters, bool p_allow_empty = true);
String join(const String &str,Vector<String> parts);


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

} // end of StringUtils namespace


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
    _FORCE_INLINE_ uint32_t operator()(const String &p_string) { return StringUtils::hash(p_string); }
};
