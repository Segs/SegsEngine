#pragma once
#include "core/typedefs.h"
#include "core/hashfuncs.h"
#include "core/forward_decls.h"

class QChar;
class QByteArray;
class Variant;
class Array;
using CharType = QChar;
using CharString = QByteArray;

namespace StringUtils
{
enum Compare {
    CaseSensitive=0,
    CaseInsensitive,
    CaseNatural
};
//! length in codepoints, not bytes
[[nodiscard]] int char_length(const String &str);

[[nodiscard]] GODOT_EXPORT double to_double(se_string_view p_str);
[[nodiscard]] inline float to_float(se_string_view str) { return float(to_double(str)); }
[[nodiscard]] GODOT_EXPORT int hex_to_int(const String &str,bool p_with_prefix = true);
[[nodiscard]] GODOT_EXPORT int hex_to_int(se_string_view s,bool p_with_prefix=true);
[[nodiscard]] GODOT_EXPORT int to_int(const char *p_str, int p_len = -1);
[[nodiscard]] GODOT_EXPORT int to_int(se_string_view p_str);
[[nodiscard]] GODOT_EXPORT int64_t to_int(const CharType *p_str, int p_len = -1);
[[nodiscard]] GODOT_EXPORT int64_t to_int64(const String &str);
[[nodiscard]] GODOT_EXPORT int64_t to_int64(se_string_view str);

[[nodiscard]] GODOT_EXPORT se_string format(se_string_view str,const Variant &values);

[[nodiscard]] GODOT_EXPORT se_string num(double p_num, int p_decimals = -1);
[[nodiscard]] GODOT_EXPORT se_string num_scientific(double p_num);
[[nodiscard]] GODOT_EXPORT se_string num_real(double p_num);
[[nodiscard]] GODOT_EXPORT se_string num_int64(int64_t p_num, int base = 10, bool capitalize_hex = false);
[[nodiscard]] GODOT_EXPORT se_string num_uint64(uint64_t p_num, int base = 10, bool capitalize_hex = false);
[[nodiscard]] GODOT_EXPORT se_string md5(const uint8_t *p_md5);
[[nodiscard]] GODOT_EXPORT se_string hex_encode_buffer(const uint8_t *p_buffer, int p_len);

[[nodiscard]] GODOT_EXPORT float similarity(const String &lhs,const String &p_string);
[[nodiscard]] GODOT_EXPORT Vector<String> bigrams(const String &str);
[[nodiscard]] GODOT_EXPORT float similarity(se_string_view lhs,se_string_view p_string);
[[nodiscard]] GODOT_EXPORT Vector<se_string_view> bigrams(se_string_view str);
[[nodiscard]] GODOT_EXPORT int compare(const String &lhs,const String &rhs,Compare case_sensitive=CaseSensitive);
[[nodiscard]] GODOT_EXPORT int compare(se_string_view lhs,se_string_view rhs,Compare case_sensitive=CaseSensitive);
[[nodiscard]] GODOT_EXPORT String percent_encode(const String &src);
[[nodiscard]] GODOT_EXPORT String percent_decode(const String &src);
[[nodiscard]] GODOT_EXPORT se_string percent_encode(se_string_view src);
[[nodiscard]] GODOT_EXPORT se_string percent_decode(se_string_view str);

[[nodiscard]] GODOT_EXPORT se_string md5_text(se_string_view str);
[[nodiscard]] GODOT_EXPORT se_string md5_text(const String &str);
[[nodiscard]] GODOT_EXPORT se_string sha1_text(se_string_view str);
[[nodiscard]] GODOT_EXPORT se_string sha1_text(const String &str);
[[nodiscard]] GODOT_EXPORT se_string sha256_text(se_string_view str);
//[[nodiscard]] GODOT_EXPORT se_string sha256_text(const String &str);
[[nodiscard]] GODOT_EXPORT PODVector<uint8_t> md5_buffer(se_string_view str);
[[nodiscard]] GODOT_EXPORT Vector<uint8_t> sha1_buffer(const String &str);
[[nodiscard]] GODOT_EXPORT Vector<uint8_t> sha256_buffer(const String &str);

[[nodiscard]] GODOT_EXPORT Vector<uint8_t> sha1_buffer(se_string_view str);
[[nodiscard]] GODOT_EXPORT Vector<uint8_t> sha256_buffer(se_string_view str);

[[nodiscard]] GODOT_EXPORT CharType char_uppercase(CharType p_char);
[[nodiscard]] GODOT_EXPORT CharType char_lowercase(CharType p_char);
[[nodiscard]] GODOT_EXPORT char char_lowercase(char p_char);
[[nodiscard]] GODOT_EXPORT char char_uppercase(char p_char);

[[nodiscard]] GODOT_EXPORT se_string to_lower(se_string_view str);
[[nodiscard]] GODOT_EXPORT se_string to_upper(se_string_view str);

[[nodiscard]] GODOT_EXPORT String xml_escape(const String &src,bool p_escape_quotes = false);
[[nodiscard]] GODOT_EXPORT se_string xml_escape(se_string_view arg, bool p_escape_quotes=false);
[[nodiscard]] GODOT_EXPORT String xml_unescape(const String &src);
[[nodiscard]] GODOT_EXPORT se_string xml_unescape(se_string_view src);
//[[nodiscard]] GODOT_EXPORT String http_escape(const String &src);
//[[nodiscard]] GODOT_EXPORT String http_unescape(const String &src);
//[[nodiscard]] GODOT_EXPORT String c_escape(const String &src);
//[[nodiscard]] GODOT_EXPORT String c_escape_multiline(const String &src);
//[[nodiscard]] GODOT_EXPORT String c_unescape(const String &src);
//[[nodiscard]] GODOT_EXPORT String json_escape(const String &src);
[[nodiscard]] GODOT_EXPORT String word_wrap(const String &src,int p_chars_per_line);

[[nodiscard]] GODOT_EXPORT se_string xml_unescape(se_string_view src);
[[nodiscard]] GODOT_EXPORT se_string http_escape(se_string_view src);
[[nodiscard]] GODOT_EXPORT se_string http_unescape(se_string_view src);
[[nodiscard]] GODOT_EXPORT se_string c_escape(se_string_view src);
[[nodiscard]] GODOT_EXPORT se_string c_escape_multiline(se_string_view src);
[[nodiscard]] GODOT_EXPORT se_string c_unescape(se_string_view src);
[[nodiscard]] GODOT_EXPORT se_string json_escape(se_string_view src);
[[nodiscard]] GODOT_EXPORT se_string property_name_encode(se_string_view str);

[[nodiscard]] GODOT_EXPORT bool is_subsequence_of(const String &str,const String &p_string, Compare mode=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool is_subsequence_of(se_string_view str,se_string_view p_string, Compare mode=CaseSensitive);

[[nodiscard]] GODOT_EXPORT String join(const String &str, const Vector<String> &parts);
[[nodiscard]] GODOT_EXPORT String repeat(const String &str,int p_count);
[[nodiscard]] GODOT_EXPORT Vector<String> rsplit(const String &str,const String &p_splitter, bool p_allow_empty = true, int p_maxsplit = 0);
[[nodiscard]] GODOT_EXPORT Vector<se_string_view> rsplit(se_string_view str,se_string_view p_splitter, bool p_allow_empty = true, int p_maxsplit = 0);
[[nodiscard]] GODOT_EXPORT Vector<String> split(const String &str,CharType p_splitter, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT Vector<String> split(const String &str,const String &p_splitter, bool p_allow_empty = true, int p_maxsplit = 0);
[[nodiscard]] GODOT_EXPORT Vector<se_string_view> split(se_string_view str,se_string_view p_splitter, bool p_allow_empty = true, int p_maxsplit = 0);
[[nodiscard]] GODOT_EXPORT Vector<se_string_view> split(se_string_view str,char p_splitter, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT Vector<String> split_spaces(const String &str);
//[[nodiscard]] GODOT_EXPORT Vector<float> split_floats(const String &str,const String &p_splitter, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT Vector<float> split_floats(se_string_view str,se_string_view p_splitter, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT PODVector<float> split_floats_mk(se_string_view str,se_string_view split_chars, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT Vector<se_string_view> split_spaces(se_string_view str);
[[nodiscard]] GODOT_EXPORT bool contains(const String &heystack,CharType c,Compare mode=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool contains(const String &heystack,const String &needle,Compare mode=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool contains(const String &heystack, se_string_view needle,Compare mode=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool contains(const char *heystack, const char *needle);
[[nodiscard]] GODOT_EXPORT bool contains(se_string_view heystack, char c);
[[nodiscard]] GODOT_EXPORT bool contains(se_string_view heystack, se_string_view c);
[[nodiscard]] GODOT_EXPORT bool is_numeric(const se_string &str);
[[nodiscard]] GODOT_EXPORT bool is_quoted(const String &str);
[[nodiscard]] GODOT_EXPORT bool is_quoted(se_string_view str);
[[nodiscard]] GODOT_EXPORT bool is_valid_filename(const String &src);
[[nodiscard]] GODOT_EXPORT bool is_valid_filename(se_string_view src);
//[[nodiscard]] GODOT_EXPORT bool is_valid_float(const String &src);
[[nodiscard]] GODOT_EXPORT bool is_valid_float(se_string_view src);
[[nodiscard]] GODOT_EXPORT bool is_valid_hex_number(se_string_view src,bool p_with_prefix);
[[nodiscard]] GODOT_EXPORT bool is_valid_html_color(se_string_view src);
[[nodiscard]] GODOT_EXPORT bool is_valid_identifier(const String &src);
[[nodiscard]] GODOT_EXPORT bool is_valid_identifier(se_string_view src);
[[nodiscard]] GODOT_EXPORT bool is_valid_integer(const String &src);
[[nodiscard]] GODOT_EXPORT bool is_valid_integer(se_string_view str);
[[nodiscard]] GODOT_EXPORT bool is_valid_ip_address(se_string_view src);
[[nodiscard]] GODOT_EXPORT se_string repeat(se_string_view str,int p_count);

[[nodiscard]] GODOT_EXPORT se_string quote(se_string_view str,char character = '\"');
[[nodiscard]] GODOT_EXPORT String quote(const String &str,char character = '\"');
[[nodiscard]] GODOT_EXPORT String unquote(const String &str);
[[nodiscard]] GODOT_EXPORT se_string_view unquote(se_string_view str);

//static_assert (sizeof(CharType)==sizeof(uint16_t), "Hash functions assume CharType's underlying type is uint16_t");
// String hashing helpers
static inline uint32_t hash(const CharType *p_cstr, int p_len) {
    return hash_djb2_buffer((const uint16_t *)p_cstr,p_len);
}
static inline uint32_t hash(const CharType *p_cstr) {
    return hash_djb2((const uint16_t *)p_cstr);
}
static inline uint32_t hash(const char *p_cstr, int p_len) {
    return eastl::hash<se_string_view>()(se_string_view(p_cstr,p_len));
}
static inline uint32_t hash(const char *p_cstr) {
    return eastl::hash<se_string_view>()(se_string_view(p_cstr));
}

[[nodiscard]] GODOT_EXPORT bool begins_with(const String &str,CharType ch);
[[nodiscard]] GODOT_EXPORT bool begins_with(const String &str,const String &p_string);
[[nodiscard]] GODOT_EXPORT bool begins_with(const String &str,const char *p_string);
[[nodiscard]] GODOT_EXPORT bool ends_with(const String &str,const String &p_string);
[[nodiscard]] GODOT_EXPORT bool ends_with(const String &str,const char *p_string);
[[nodiscard]] GODOT_EXPORT bool ends_with(const String &s, CharType ch);

[[nodiscard]] GODOT_EXPORT bool begins_with(se_string_view s,se_string_view p_string);
[[nodiscard]] GODOT_EXPORT bool ends_with(se_string_view s,se_string_view p_string);
[[nodiscard]] GODOT_EXPORT bool ends_with(se_string_view s,char c);

[[nodiscard]] GODOT_EXPORT int64_t hex_to_int64(const String &str,bool p_with_prefix = true);
[[nodiscard]] GODOT_EXPORT int64_t hex_to_int64(se_string_view s,bool p_with_prefix = true);
[[nodiscard]] GODOT_EXPORT int64_t bin_to_int64(const String &str,bool p_with_prefix = true);
[[nodiscard]] GODOT_EXPORT int64_t bin_to_int64(se_string_view str,bool p_with_prefix = true);

[[nodiscard]] GODOT_EXPORT String capitalize(const String &str);
[[nodiscard]] GODOT_EXPORT se_string capitalize(se_string_view str);
[[nodiscard]] GODOT_EXPORT String camelcase_to_underscore(const String &str,bool lowercase = true);
[[nodiscard]] GODOT_EXPORT se_string camelcase_to_underscore(se_string_view str,bool lowercase = true);

[[nodiscard]] GODOT_EXPORT int count(const String &heystack,const String &p_string, int p_from = 0, int p_to = 0);
[[nodiscard]] GODOT_EXPORT int countn(const String &heystack,const String &p_string, int p_from = 0, int p_to = 0);
[[nodiscard]] GODOT_EXPORT int count(se_string_view heystack,se_string_view p_string, int p_from = 0, int p_to = 0);
[[nodiscard]] GODOT_EXPORT int countn(se_string_view heystack,se_string_view p_string, int p_from = 0, int p_to = 0);

[[nodiscard]] GODOT_EXPORT int get_slice_count(const String &str, const String &p_splitter);
[[nodiscard]] GODOT_EXPORT int get_slice_count(const String &str,CharType p_splitter);
[[nodiscard]] GODOT_EXPORT int get_slice_count(se_string_view str,char p_splitter);
[[nodiscard]] GODOT_EXPORT int get_slice_count(se_string_view str,se_string_view p_splitter);

//[[nodiscard]] GODOT_EXPORT String get_slice(const String &str, const String &p_splitter, int p_slice);
//[[nodiscard]] GODOT_EXPORT String get_slice(const String &str, se_string_view p_splitter, int p_slice);
//[[nodiscard]] GODOT_EXPORT String get_slice(const String &str,CharType p_splitter, int p_slice) ;
[[nodiscard]] GODOT_EXPORT se_string_view get_slice(se_string_view str,char p_splitter, int p_slice);
[[nodiscard]] GODOT_EXPORT se_string_view get_slice(se_string_view str,se_string_view p_splitter, int p_slice);

//[[nodiscard]] GODOT_EXPORT String dedent(const String &str);
[[nodiscard]] GODOT_EXPORT String left(const String &str,int p_pos);
[[nodiscard]] GODOT_EXPORT String lstrip(const String &str,const String &p_chars);
[[nodiscard]] GODOT_EXPORT String right(const String &str,int p_pos);
[[nodiscard]] GODOT_EXPORT String rstrip(const String &str,const String &p_chars);
[[nodiscard]] GODOT_EXPORT String strip_edges(const String &str,bool left = true, bool right = true);
[[nodiscard]] GODOT_EXPORT String strip_escapes(const String &str);
[[nodiscard]] GODOT_EXPORT String substr(const String &str,int p_from, int p_chars = -1);
[[nodiscard]] GODOT_EXPORT bool match(const String &str,const String &p_wildcard,Compare sensitivity=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool match(se_string_view str,se_string_view p_wildcard,Compare sensitivity=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool matchn(const String &str,const String &p_wildcard);
[[nodiscard]] GODOT_EXPORT bool matchn(se_string_view str,se_string_view p_wildcard);
[[nodiscard]] GODOT_EXPORT int find(const String &str,const String &p_str, int p_from = 0); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int find(const String &str,const char *p_str, int p_from = 0); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT size_t find(se_string_view str,se_string_view p_str, size_t p_from = 0); ///< return se_string::npos if failed
[[nodiscard]] GODOT_EXPORT size_t find(se_string_view str,char p_str, size_t p_from = 0); ///< return se_string::npos if failed
[[nodiscard]] GODOT_EXPORT size_t find_last(se_string_view str,se_string_view p_str); ///< return se_string::npos if failed
[[nodiscard]] GODOT_EXPORT size_t find_last(se_string_view str,char p_ch); ///< return se_string::npos if failed
[[nodiscard]] GODOT_EXPORT int find_last(const String &str,CharType p_ch); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int find_last(const String &str,const String &p_str); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int findn(const String &str,const String &p_str, int p_from = 0); ///< return <0 if failed, case insensitive
[[nodiscard]] GODOT_EXPORT size_t findn(se_string_view str,se_string_view p_str, int p_from = 0); ///< return <0 if failed, case insensitive
[[nodiscard]] GODOT_EXPORT int rfind(const String &str,const String &p_str, int p_from = -1); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT size_t rfind(se_string_view str,se_string_view p_str, int p_from = -1); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT size_t rfind(se_string_view str,char p_str, int p_from = -1); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int rfindn(const String &str,const String &p_str, int p_from = -1); ///< return <0 if failed, case insensitive
[[nodiscard]] GODOT_EXPORT size_t rfindn(se_string_view str,se_string_view p_str, int p_from = -1); ///< return <0 if failed, case insensitive
[[nodiscard]] GODOT_EXPORT se_string_view left(se_string_view s,int p_pos);
[[nodiscard]] GODOT_EXPORT se_string_view right(se_string_view str,int p_pos);
[[nodiscard]] GODOT_EXPORT se_string strip_escapes(se_string_view tr);
[[nodiscard]] GODOT_EXPORT se_string_view lstrip(se_string_view str,se_string_view p_chars);
[[nodiscard]] GODOT_EXPORT se_string_view rstrip(se_string_view str,se_string_view p_chars);
[[nodiscard]] GODOT_EXPORT se_string_view strip_edges(se_string_view str, bool left = true, bool right = true);
[[nodiscard]] GODOT_EXPORT se_string_view substr(se_string_view str,int p_from, size_t p_chars = size_t(-1));
[[nodiscard]] GODOT_EXPORT se_string dedent(se_string_view str);

// Grapheme and locale specific helpers
[[nodiscard]] GODOT_EXPORT int grapheme_count(se_string_view str);
[[nodiscard]] GODOT_EXPORT int bytes_in_next_grapheme(se_string_view str,int offset=0);


/* complex helpers */

[[nodiscard]] CharType ord_at(const String &str,int p_idx);

void erase(String &str,int p_pos, int p_chars);
void erase(se_string &str,int p_pos, int p_chars);

[[nodiscard]] GODOT_EXPORT CharString ascii(const String &str,bool p_allow_extended = false);
[[nodiscard]] GODOT_EXPORT CharString utf8(const String &str);
GODOT_EXPORT bool parse_utf8(String &str,const char *p_utf8, int p_len = -1); //return true on error
[[nodiscard]] GODOT_EXPORT String from_utf8(const char *sv, int p_len= - 1);
[[nodiscard]] GODOT_EXPORT String from_utf8(se_string_view sv);
[[nodiscard]] GODOT_EXPORT String from_wchar(const wchar_t *p_utf8, int p_len = -1);
[[nodiscard]] GODOT_EXPORT se_string replace(se_string_view str,se_string_view p_key, se_string_view p_with);
[[nodiscard]] GODOT_EXPORT se_string replace(se_string_view str,char p_key, char p_with);

namespace Inplace {
    GODOT_EXPORT void replace(String &str,const String &p_key, const String &p_with);
    GODOT_EXPORT void replace(String &str,CharType p_key, CharType p_with);
    GODOT_EXPORT void replace(String &s,int i,int len, const String &p_after);
    GODOT_EXPORT void replace(String &str,const char *p_key, const char *p_with);
}
[[nodiscard]] GODOT_EXPORT String insert(const String &str,int p_at_pos, const String &p_string);
[[nodiscard]] GODOT_EXPORT String lpad(const String &str,int min_length, char character = ' ');
//[[nodiscard]] GODOT_EXPORT String pad_decimals(const String &str,int p_digits);
[[nodiscard]] GODOT_EXPORT String pad_zeros(const String &str,int p_digits);
[[nodiscard]] GODOT_EXPORT String replace_first(const String &str,const String &p_key, const String &p_with);
[[nodiscard]] GODOT_EXPORT String replacen(const String &str,const String &p_key, const String &p_with);
[[nodiscard]] GODOT_EXPORT String rpad(const String &str,int min_length, char character = ' ');
//[[nodiscard]] GODOT_EXPORT String sprintf(const String &str,const Array &values, bool *error);
[[nodiscard]] GODOT_EXPORT se_string sprintf(se_string_view str,const Array &values, bool *error);
[[nodiscard]] GODOT_EXPORT String trim_prefix(const String &str,const String &p_prefix);
[[nodiscard]] GODOT_EXPORT String trim_suffix(const String &str,const String &p_suffix);

[[nodiscard]] GODOT_EXPORT se_string insert(se_string_view str,int p_at_pos, se_string_view p_string);
[[nodiscard]] GODOT_EXPORT se_string lpad(const se_string &src,int min_length, char character = ' ');
[[nodiscard]] GODOT_EXPORT se_string pad_decimals(se_string_view str,int p_digits);
[[nodiscard]] GODOT_EXPORT se_string pad_zeros(se_string_view str,int p_digits);
[[nodiscard]] GODOT_EXPORT se_string replace_first(se_string_view str, se_string_view p_key, se_string_view p_with);
[[nodiscard]] GODOT_EXPORT se_string replacen(se_string_view str,se_string_view p_key, se_string_view p_with);
[[nodiscard]] GODOT_EXPORT se_string rpad(const se_string &src,int min_length, char character = ' ');
[[nodiscard]] GODOT_EXPORT se_string_view trim_prefix(se_string_view str,se_string_view p_prefix);
[[nodiscard]] GODOT_EXPORT se_string_view trim_suffix(se_string_view str,se_string_view p_suffix);

} // end of StringUtils namespace

namespace PathUtils
{
    [[nodiscard]] GODOT_EXPORT se_string_view get_extension(se_string_view p);
    [[nodiscard]] GODOT_EXPORT String get_extension(const String &p);
    [[nodiscard]] GODOT_EXPORT se_string_view get_basename(se_string_view p);
    //! Returns a path to give file: /a/path/to/file -> /a/path/to  file -> .
    [[nodiscard]] GODOT_EXPORT se_string_view path(se_string_view p);
    [[nodiscard]] GODOT_EXPORT String get_basename(const String &p);
    [[nodiscard]] GODOT_EXPORT se_string plus_file(se_string_view bp,se_string_view p_file);
    //! @note for now it just replaces \\ with /
    [[nodiscard]] GODOT_EXPORT se_string from_native_path(se_string_view p);
    [[nodiscard]] GODOT_EXPORT String from_native_path(const String &p);
    [[nodiscard]] GODOT_EXPORT se_string to_win_path(const se_string &v);
    [[nodiscard]] GODOT_EXPORT String to_win_path(const String &v);
    // path functions
    [[nodiscard]] GODOT_EXPORT bool is_internal_path(se_string_view path);
    [[nodiscard]] GODOT_EXPORT bool is_internal_path(const String &path);
    [[nodiscard]] GODOT_EXPORT bool is_abs_path(const String &p);
    [[nodiscard]] GODOT_EXPORT bool is_abs_path(se_string_view p);
    [[nodiscard]] GODOT_EXPORT bool is_rel_path(const String &p);
    [[nodiscard]] GODOT_EXPORT bool is_rel_path(se_string_view p);
    [[nodiscard]] GODOT_EXPORT bool is_resource_file(const String &p);
    [[nodiscard]] GODOT_EXPORT bool is_resource_file(se_string_view p);
    [[nodiscard]] GODOT_EXPORT se_string path_to(se_string_view p, se_string_view  p_path);
    [[nodiscard]] GODOT_EXPORT se_string path_to_file(se_string_view p, se_string_view  p_path);
    [[nodiscard]] GODOT_EXPORT se_string get_base_dir(se_string_view path);
    [[nodiscard]] GODOT_EXPORT se_string_view get_file(se_string_view p);
    [[nodiscard]] GODOT_EXPORT se_string humanize_size(uint64_t p_size);
    [[nodiscard]] GODOT_EXPORT se_string_view trim_trailing_slash(se_string_view path);
    [[nodiscard]] GODOT_EXPORT se_string_view get_file(se_string_view path);
    [[nodiscard]] GODOT_EXPORT se_string simplify_path(se_string_view str);
    //[[nodiscard]] GODOT_EXPORT String trim_trailing_slash(const String &path);

} // end o PathUtils namespace


GODOT_EXPORT se_string itos(int64_t p_val);
GODOT_EXPORT se_string rtos(double p_val);
GODOT_EXPORT se_string rtoss(double p_val); // scientific version

struct NoCaseComparator {

    bool operator()(const String &p_a, const String &p_b) const noexcept {

        return StringUtils::compare(p_a, p_b, StringUtils::CaseInsensitive) < 0;
    }
};

struct NaturalNoCaseComparator {

    bool operator()(const String &p_a, const String &p_b) const {

        return StringUtils::compare(p_a, p_b, StringUtils::CaseNatural) < 0;
    }
    bool operator()(se_string_view p_a, se_string_view p_b) const {

        return StringUtils::compare(p_a, p_b, StringUtils::CaseNatural) < 0;
    }
};

/* end of namespace */

bool is_symbol(CharType c);
bool select_word(const String &p_s, int p_col, int &r_beg, int &r_end);

