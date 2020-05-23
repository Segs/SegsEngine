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
[[nodiscard]] int char_length(const UIString &str);
[[nodiscard]] GODOT_EXPORT double to_double(StringView p_str);
[[nodiscard]] GODOT_EXPORT double to_double(const char *p_str, char ** r_end);
[[nodiscard]] inline float to_float(StringView str) { return float(to_double(str)); }
[[nodiscard]] GODOT_EXPORT int hex_to_int(const UIString &str,bool p_with_prefix = true);
[[nodiscard]] GODOT_EXPORT int hex_to_int(StringView s,bool p_with_prefix=true);
[[nodiscard]] GODOT_EXPORT int to_int(const char *p_str, int p_len = -1);
[[nodiscard]] GODOT_EXPORT int to_int(StringView p_str);
[[nodiscard]] GODOT_EXPORT int64_t to_int(const CharType *p_str, int p_len = -1);
[[nodiscard]] GODOT_EXPORT int64_t to_int64(const UIString &str);
[[nodiscard]] GODOT_EXPORT int64_t to_int64(StringView str);

[[nodiscard]] GODOT_EXPORT String format(StringView str,const Variant &values);

[[nodiscard]] GODOT_EXPORT String num(double p_num, int p_decimals = -1);
[[nodiscard]] GODOT_EXPORT String num_scientific(double p_num);
[[nodiscard]] GODOT_EXPORT String num_real(double p_num);
[[nodiscard]] GODOT_EXPORT String num_int64(int64_t p_num, int base = 10, bool capitalize_hex = false);
[[nodiscard]] GODOT_EXPORT String num_uint64(uint64_t p_num, int base = 10, bool capitalize_hex = false);
[[nodiscard]] GODOT_EXPORT String md5(const uint8_t *p_md5);
[[nodiscard]] GODOT_EXPORT String hex_encode_buffer(const uint8_t *p_buffer, int p_len);

[[nodiscard]] GODOT_EXPORT float similarity(StringView lhs,StringView p_string);
[[nodiscard]] GODOT_EXPORT Vector<StringView> bigrams(StringView str);
[[nodiscard]] GODOT_EXPORT int compare(const UIString &lhs,const UIString &rhs,Compare case_sensitive=CaseSensitive);
[[nodiscard]] GODOT_EXPORT int compare(StringView lhs,StringView rhs,Compare case_sensitive=CaseSensitive);
[[nodiscard]] GODOT_EXPORT UIString percent_encode(const UIString &src);
[[nodiscard]] GODOT_EXPORT UIString percent_decode(const UIString &src);
[[nodiscard]] GODOT_EXPORT String percent_encode(StringView src);
[[nodiscard]] GODOT_EXPORT String percent_decode(StringView str);

[[nodiscard]] GODOT_EXPORT String md5_text(StringView str);
[[nodiscard]] GODOT_EXPORT String md5_text(const UIString &str);
[[nodiscard]] GODOT_EXPORT String sha1_text(StringView str);
[[nodiscard]] GODOT_EXPORT String sha1_text(const UIString &str);
[[nodiscard]] GODOT_EXPORT String sha256_text(StringView str);
//[[nodiscard]] GODOT_EXPORT String sha256_text(const String &str);
[[nodiscard]] GODOT_EXPORT Vector<uint8_t> md5_buffer(StringView str);
[[nodiscard]] GODOT_EXPORT Vector<uint8_t> sha1_buffer(const UIString &str);
[[nodiscard]] GODOT_EXPORT Vector<uint8_t> sha256_buffer(const UIString &str);

[[nodiscard]] GODOT_EXPORT Vector<uint8_t> sha1_buffer(StringView str);
[[nodiscard]] GODOT_EXPORT Vector<uint8_t> sha256_buffer(StringView str);

[[nodiscard]] GODOT_EXPORT CharType char_uppercase(CharType p_char);
[[nodiscard]] GODOT_EXPORT CharType char_lowercase(CharType p_char);
[[nodiscard]] GODOT_EXPORT char char_lowercase(char p_char);
[[nodiscard]] GODOT_EXPORT char char_uppercase(char p_char);

[[nodiscard]] GODOT_EXPORT String to_lower(StringView str);
[[nodiscard]] GODOT_EXPORT String to_upper(StringView str);

[[nodiscard]] GODOT_EXPORT UIString xml_escape(const UIString &src,bool p_escape_quotes = false);
[[nodiscard]] GODOT_EXPORT String xml_escape(StringView arg, bool p_escape_quotes=false);
[[nodiscard]] GODOT_EXPORT UIString xml_unescape(const UIString &src);
[[nodiscard]] GODOT_EXPORT String xml_unescape(StringView src);
//[[nodiscard]] GODOT_EXPORT String http_escape(const String &src);
//[[nodiscard]] GODOT_EXPORT String http_unescape(const String &src);
//[[nodiscard]] GODOT_EXPORT String c_escape(const String &src);
//[[nodiscard]] GODOT_EXPORT String c_escape_multiline(const String &src);
//[[nodiscard]] GODOT_EXPORT String c_unescape(const String &src);
//[[nodiscard]] GODOT_EXPORT String json_escape(const String &src);
[[nodiscard]] GODOT_EXPORT UIString word_wrap(const UIString &src,int p_chars_per_line);

[[nodiscard]] GODOT_EXPORT String xml_unescape(StringView src);
[[nodiscard]] GODOT_EXPORT String http_escape(StringView src);
[[nodiscard]] GODOT_EXPORT String http_unescape(StringView src);
[[nodiscard]] GODOT_EXPORT String c_escape(StringView src);
[[nodiscard]] GODOT_EXPORT String c_escape_multiline(StringView src);
[[nodiscard]] GODOT_EXPORT String c_unescape(StringView src);
[[nodiscard]] GODOT_EXPORT String json_escape(StringView src);
[[nodiscard]] GODOT_EXPORT String property_name_encode(StringView str);

[[nodiscard]] GODOT_EXPORT bool is_subsequence_of(const UIString &str,const UIString &p_string, Compare mode=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool is_subsequence_of(StringView str,StringView p_string, Compare mode=CaseSensitive);

[[nodiscard]] GODOT_EXPORT UIString join(const UIString &str, const Vector<UIString> &parts);
[[nodiscard]] GODOT_EXPORT UIString repeat(const UIString &str,int p_count);
[[nodiscard]] GODOT_EXPORT Vector<UIString> rsplit(const UIString &str,const UIString &p_splitter, bool p_allow_empty = true, int p_maxsplit = 0);
[[nodiscard]] GODOT_EXPORT Vector<StringView> rsplit(StringView str,StringView p_splitter, bool p_allow_empty = true, int p_maxsplit = 0);
[[nodiscard]] GODOT_EXPORT Vector<UIString> split(const UIString &str,CharType p_splitter, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT Vector<UIString> split(const UIString &str,const UIString &p_splitter, bool p_allow_empty = true, int p_maxsplit = 0);
[[nodiscard]] GODOT_EXPORT Vector<StringView> split(StringView str,StringView p_splitter, bool p_allow_empty = true, int p_maxsplit = 0);
[[nodiscard]] GODOT_EXPORT Vector<StringView> split(StringView str,char p_splitter, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT Vector<UIString> split_spaces(const UIString &str);
//[[nodiscard]] GODOT_EXPORT Vector<float> split_floats(const String &str,const String &p_splitter, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT Vector<float> split_floats(StringView str,StringView p_splitter, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT Vector<float> split_floats_mk(StringView str,StringView split_chars, bool p_allow_empty = true);
[[nodiscard]] GODOT_EXPORT Vector<StringView> split_spaces(StringView str);
[[nodiscard]] GODOT_EXPORT bool contains(const UIString &heystack,CharType c,Compare mode=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool contains(const UIString &heystack,const UIString &needle,Compare mode=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool contains(const UIString &heystack, StringView needle,Compare mode=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool contains(const char *heystack, const char *needle);
[[nodiscard]] GODOT_EXPORT bool contains(StringView heystack, char c);
[[nodiscard]] GODOT_EXPORT bool contains(StringView heystack, StringView c);
[[nodiscard]] GODOT_EXPORT bool is_numeric(const String &str);
[[nodiscard]] GODOT_EXPORT bool is_quoted(const UIString &str);
[[nodiscard]] GODOT_EXPORT bool is_quoted(StringView str);
[[nodiscard]] GODOT_EXPORT bool is_valid_filename(const UIString &src);
[[nodiscard]] GODOT_EXPORT bool is_valid_filename(StringView src);
//[[nodiscard]] GODOT_EXPORT bool is_valid_float(const String &src);
[[nodiscard]] GODOT_EXPORT bool is_valid_float(StringView src);
[[nodiscard]] GODOT_EXPORT bool is_valid_hex_number(StringView src,bool p_with_prefix);
[[nodiscard]] GODOT_EXPORT bool is_valid_html_color(StringView src);
[[nodiscard]] GODOT_EXPORT bool is_valid_identifier(const UIString &src);
[[nodiscard]] GODOT_EXPORT bool is_valid_identifier(StringView src);
[[nodiscard]] GODOT_EXPORT bool is_valid_integer(const UIString &src);
[[nodiscard]] GODOT_EXPORT bool is_valid_integer(StringView str);
[[nodiscard]] GODOT_EXPORT bool is_valid_ip_address(StringView src);
[[nodiscard]] GODOT_EXPORT String repeat(StringView str,int p_count);

[[nodiscard]] GODOT_EXPORT String quote(StringView str,char character = '\"');
[[nodiscard]] GODOT_EXPORT UIString quote(const UIString &str,char character = '\"');
[[nodiscard]] GODOT_EXPORT UIString unquote(const UIString &str);
[[nodiscard]] GODOT_EXPORT StringView unquote(StringView str);

//static_assert (sizeof(CharType)==sizeof(uint16_t), "Hash functions assume CharType's underlying type is uint16_t");
// String hashing helpers
static inline uint32_t hash(const CharType *p_cstr, int p_len) {
    return hash_djb2_buffer((const uint16_t *)p_cstr,p_len);
}
static inline uint32_t hash(const CharType *p_cstr) {
    return hash_djb2((const uint16_t *)p_cstr);
}
static inline uint32_t hash(const char *p_cstr, int p_len) {
    return uint32_t(eastl::hash<StringView>()(StringView(p_cstr,p_len)));
}
static inline uint32_t hash(const char *p_cstr) {
    return eastl::hash<StringView>()(StringView(p_cstr));
}

[[nodiscard]] GODOT_EXPORT bool begins_with(const UIString &str,CharType ch);
[[nodiscard]] GODOT_EXPORT bool begins_with(const UIString &str,const UIString &p_string);
[[nodiscard]] GODOT_EXPORT bool begins_with(const UIString &str,const char *p_string);
[[nodiscard]] GODOT_EXPORT bool ends_with(const UIString &str,const UIString &p_string);
[[nodiscard]] GODOT_EXPORT bool ends_with(const UIString &str,const char *p_string);
[[nodiscard]] GODOT_EXPORT bool ends_with(const UIString &s, CharType ch);

[[nodiscard]] GODOT_EXPORT bool begins_with(StringView s,StringView p_string);
[[nodiscard]] GODOT_EXPORT bool ends_with(StringView s,StringView p_string);
[[nodiscard]] GODOT_EXPORT bool ends_with(StringView s,char c);

[[nodiscard]] GODOT_EXPORT int64_t hex_to_int64(const UIString &str,bool p_with_prefix = true);
[[nodiscard]] GODOT_EXPORT int64_t hex_to_int64(StringView s,bool p_with_prefix = true);
[[nodiscard]] GODOT_EXPORT int64_t bin_to_int64(const UIString &str,bool p_with_prefix = true);
[[nodiscard]] GODOT_EXPORT int64_t bin_to_int64(StringView str,bool p_with_prefix = true);

[[nodiscard]] GODOT_EXPORT UIString capitalize(const UIString &str);
[[nodiscard]] GODOT_EXPORT String capitalize(StringView str);
[[nodiscard]] GODOT_EXPORT UIString camelcase_to_underscore(const UIString &str,bool lowercase = true);
[[nodiscard]] GODOT_EXPORT String camelcase_to_underscore(StringView str,bool lowercase = true);

[[nodiscard]] GODOT_EXPORT int count(const UIString &heystack,const UIString &p_string, int p_from = 0, int p_to = 0);
[[nodiscard]] GODOT_EXPORT int countn(const UIString &heystack,const UIString &p_string, int p_from = 0, int p_to = 0);
[[nodiscard]] GODOT_EXPORT int count(StringView heystack,StringView p_string, int p_from = 0, int p_to = 0);
[[nodiscard]] GODOT_EXPORT int countn(StringView heystack,StringView p_string, int p_from = 0, int p_to = 0);

[[nodiscard]] GODOT_EXPORT int get_slice_count(const UIString &str, const UIString &p_splitter);
[[nodiscard]] GODOT_EXPORT int get_slice_count(const UIString &str,CharType p_splitter);
[[nodiscard]] GODOT_EXPORT int get_slice_count(StringView str,char p_splitter);
[[nodiscard]] GODOT_EXPORT int get_slice_count(StringView str,StringView p_splitter);

//[[nodiscard]] GODOT_EXPORT String get_slice(const String &str, const String &p_splitter, int p_slice);
//[[nodiscard]] GODOT_EXPORT String get_slice(const String &str, StringView p_splitter, int p_slice);
//[[nodiscard]] GODOT_EXPORT String get_slice(const String &str,CharType p_splitter, int p_slice) ;
[[nodiscard]] GODOT_EXPORT StringView get_slice(StringView str,char p_splitter, int p_slice);
[[nodiscard]] GODOT_EXPORT StringView get_slice(StringView str,StringView p_splitter, int p_slice);

//[[nodiscard]] GODOT_EXPORT String dedent(const String &str);
[[nodiscard]] GODOT_EXPORT UIString left(const UIString &str,int p_pos);
[[nodiscard]] GODOT_EXPORT UIString lstrip(const UIString &str,const UIString &p_chars);
[[nodiscard]] GODOT_EXPORT UIString right(const UIString &str,int p_pos);
[[nodiscard]] GODOT_EXPORT UIString rstrip(const UIString &str,const UIString &p_chars);
[[nodiscard]] GODOT_EXPORT UIString strip_edges(const UIString &str,bool left = true, bool right = true);
[[nodiscard]] GODOT_EXPORT UIString strip_escapes(const UIString &str);
[[nodiscard]] GODOT_EXPORT UIString substr(const UIString &str,int p_from, int p_chars = -1);
[[nodiscard]] GODOT_EXPORT bool match(const UIString &str,const UIString &p_wildcard,Compare sensitivity=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool match(StringView str,StringView p_wildcard,Compare sensitivity=CaseSensitive);
[[nodiscard]] GODOT_EXPORT bool matchn(const UIString &str,const UIString &p_wildcard);
[[nodiscard]] GODOT_EXPORT bool matchn(StringView str,StringView p_wildcard);
[[nodiscard]] GODOT_EXPORT int find(const UIString &str,const UIString &p_str, int p_from = 0); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int find(const UIString &str,const char *p_str, int p_from = 0); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT size_t find(StringView str,StringView p_str, size_t p_from = 0); ///< return String::npos if failed
[[nodiscard]] GODOT_EXPORT size_t find(StringView str,char p_str, size_t p_from = 0); ///< return String::npos if failed
[[nodiscard]] GODOT_EXPORT size_t find_last(StringView str,StringView p_str); ///< return String::npos if failed
[[nodiscard]] GODOT_EXPORT size_t find_last(StringView str,char p_ch); ///< return String::npos if failed
[[nodiscard]] GODOT_EXPORT int find_last(const UIString &str,CharType p_ch); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int find_last(const UIString &str,const UIString &p_str); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int findn(const UIString &str,const UIString &p_str, int p_from = 0); ///< return <0 if failed, case insensitive
[[nodiscard]] GODOT_EXPORT size_t findn(StringView str,StringView p_str, int p_from = 0); ///< return <0 if failed, case insensitive
[[nodiscard]] GODOT_EXPORT int rfind(const UIString &str,const UIString &p_str, int p_from = -1); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT size_t rfind(StringView str,StringView p_str, int p_from = -1); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT size_t rfind(StringView str,char p_str, int p_from = -1); ///< return <0 if failed
[[nodiscard]] GODOT_EXPORT int rfindn(const UIString &str,const UIString &p_str, int p_from = -1); ///< return <0 if failed, case insensitive
[[nodiscard]] GODOT_EXPORT size_t rfindn(StringView str,StringView p_str, int p_from = -1); ///< return <0 if failed, case insensitive
[[nodiscard]] GODOT_EXPORT StringView left(StringView s,int p_pos);
[[nodiscard]] GODOT_EXPORT StringView right(StringView str,int p_pos);
[[nodiscard]] GODOT_EXPORT String strip_escapes(StringView tr);
[[nodiscard]] GODOT_EXPORT StringView lstrip(StringView str,StringView p_chars);
[[nodiscard]] GODOT_EXPORT StringView rstrip(StringView str,StringView p_chars);
[[nodiscard]] GODOT_EXPORT StringView strip_edges(StringView str, bool left = true, bool right = true);
[[nodiscard]] GODOT_EXPORT StringView substr(StringView str,int p_from, size_t p_chars = size_t(-1));
[[nodiscard]] GODOT_EXPORT String dedent(StringView str);

// Grapheme and locale specific helpers
[[nodiscard]] GODOT_EXPORT int grapheme_count(StringView str);
[[nodiscard]] GODOT_EXPORT int bytes_in_next_grapheme(StringView str,int offset=0);


/* complex helpers */

[[nodiscard]] CharType ord_at(const UIString &str,int p_idx);

void erase(UIString &str,int p_pos, int p_chars);
void erase(String &str,int p_pos, int p_chars);

[[nodiscard]] GODOT_EXPORT CharString ascii(const UIString &str,bool p_allow_extended = false);
[[nodiscard]] GODOT_EXPORT String utf8(const UIString &str);
GODOT_EXPORT bool parse_utf8(UIString &str,const char *p_utf8, int p_len = -1); //return true on error
[[nodiscard]] GODOT_EXPORT UIString from_utf8(const char *sv, int p_len= - 1);
[[nodiscard]] GODOT_EXPORT UIString from_utf8(StringView sv);
[[nodiscard]] GODOT_EXPORT UIString from_wchar(const wchar_t *p_utf8, int p_len = -1);
[[nodiscard]] GODOT_EXPORT String replace(StringView str,StringView p_key, StringView p_with);
[[nodiscard]] GODOT_EXPORT String replace(StringView str,char p_key, char p_with);

namespace Inplace {
    GODOT_EXPORT void replace(UIString &str,const UIString &p_key, const UIString &p_with);
    GODOT_EXPORT void replace(UIString &str,CharType p_key, CharType p_with);
    GODOT_EXPORT void replace(UIString &s,int i,int len, const UIString &p_after);
    GODOT_EXPORT void replace(UIString &str,const char *p_key, const char *p_with);
}
[[nodiscard]] GODOT_EXPORT UIString insert(const UIString &str,int p_at_pos, const UIString &p_string);
[[nodiscard]] GODOT_EXPORT UIString lpad(const UIString &str,int min_length, char character = ' ');
//[[nodiscard]] GODOT_EXPORT String pad_decimals(const String &str,int p_digits);
[[nodiscard]] GODOT_EXPORT UIString pad_zeros(const UIString &str,int p_digits);
[[nodiscard]] GODOT_EXPORT UIString replace_first(const UIString &str,const UIString &p_key, const UIString &p_with);
[[nodiscard]] GODOT_EXPORT UIString replacen(const UIString &str,const UIString &p_key, const UIString &p_with);
[[nodiscard]] GODOT_EXPORT UIString rpad(const UIString &str,int min_length, char character = ' ');
//[[nodiscard]] GODOT_EXPORT String sprintf(const String &str,const Array &values, bool *error);
[[nodiscard]] GODOT_EXPORT String sprintf(StringView str,const Array &values, bool *error);
[[nodiscard]] GODOT_EXPORT UIString trim_prefix(const UIString &str,const UIString &p_prefix);
[[nodiscard]] GODOT_EXPORT UIString trim_suffix(const UIString &str,const UIString &p_suffix);

[[nodiscard]] GODOT_EXPORT String insert(StringView str,int p_at_pos, StringView p_string);
[[nodiscard]] GODOT_EXPORT String lpad(const String &src,int min_length, char character = ' ');
[[nodiscard]] GODOT_EXPORT String pad_decimals(StringView str,int p_digits);
[[nodiscard]] GODOT_EXPORT String pad_zeros(StringView str,int p_digits);
[[nodiscard]] GODOT_EXPORT String replace_first(StringView str, StringView p_key, StringView p_with);
[[nodiscard]] GODOT_EXPORT String replacen(StringView str,StringView p_key, StringView p_with);
[[nodiscard]] GODOT_EXPORT String rpad(const String &src,int min_length, char character = ' ');
[[nodiscard]] GODOT_EXPORT StringView trim_prefix(StringView str,StringView p_prefix);
[[nodiscard]] GODOT_EXPORT StringView trim_suffix(StringView str,StringView p_suffix);

} // end of StringUtils namespace

namespace PathUtils
{
    [[nodiscard]] GODOT_EXPORT StringView get_extension(StringView p);
    [[nodiscard]] GODOT_EXPORT UIString get_extension(const UIString &p);
    [[nodiscard]] GODOT_EXPORT StringView get_basename(StringView p);
    //! Returns a path to give file: /a/path/to/file -> /a/path/to  file -> .
    [[nodiscard]] GODOT_EXPORT StringView path(StringView p);
    [[nodiscard]] GODOT_EXPORT UIString get_basename(const UIString &p);
    [[nodiscard]] GODOT_EXPORT String plus_file(StringView bp,StringView p_file);
    [[nodiscard]] GODOT_EXPORT String join_path(Span<StringView> parts);
    //! @note for now it just replaces \\ with /
    [[nodiscard]] GODOT_EXPORT String from_native_path(StringView p);
    [[nodiscard]] GODOT_EXPORT UIString from_native_path(const UIString &p);
    [[nodiscard]] GODOT_EXPORT String to_win_path(const String &v);
    [[nodiscard]] GODOT_EXPORT UIString to_win_path(const UIString &v);
    // path functions
    [[nodiscard]] GODOT_EXPORT bool is_internal_path(StringView path);
    [[nodiscard]] GODOT_EXPORT bool is_internal_path(const UIString &path);
    [[nodiscard]] GODOT_EXPORT bool is_abs_path(const UIString &p);
    [[nodiscard]] GODOT_EXPORT bool is_abs_path(StringView p);
    [[nodiscard]] GODOT_EXPORT bool is_rel_path(const UIString &p);
    [[nodiscard]] GODOT_EXPORT bool is_rel_path(StringView p);
    [[nodiscard]] GODOT_EXPORT bool is_resource_file(const UIString &p);
    [[nodiscard]] GODOT_EXPORT bool is_resource_file(StringView p);
    [[nodiscard]] GODOT_EXPORT String path_to(StringView p, StringView  p_path);
    [[nodiscard]] GODOT_EXPORT String path_to_file(StringView p, StringView  p_path);
    [[nodiscard]] GODOT_EXPORT String get_base_dir(StringView path);
    [[nodiscard]] GODOT_EXPORT StringView get_file(StringView p);
    [[nodiscard]] GODOT_EXPORT String humanize_size(uint64_t p_size);
    [[nodiscard]] GODOT_EXPORT StringView trim_trailing_slash(StringView path);
    [[nodiscard]] GODOT_EXPORT StringView get_file(StringView path);
    [[nodiscard]] GODOT_EXPORT String simplify_path(StringView str);
    //[[nodiscard]] GODOT_EXPORT String trim_trailing_slash(const String &path);

} // end o PathUtils namespace


GODOT_EXPORT String itos(int64_t p_val);
GODOT_EXPORT String rtos(double p_val);
GODOT_EXPORT String rtoss(double p_val); // scientific version

struct NoCaseComparator {

    bool operator()(const UIString &p_a, const UIString &p_b) const noexcept {

        return StringUtils::compare(p_a, p_b, StringUtils::CaseInsensitive) < 0;
    }
};

struct NaturalNoCaseComparator {

    bool operator()(const UIString &p_a, const UIString &p_b) const {

        return StringUtils::compare(p_a, p_b, StringUtils::CaseNatural) < 0;
    }
    bool operator()(StringView p_a, StringView p_b) const {

        return StringUtils::compare(p_a, p_b, StringUtils::CaseNatural) < 0;
    }
};

/* end of namespace */

bool is_symbol(CharType c);
bool select_word(const UIString &p_s, int p_col, int &r_beg, int &r_end);

