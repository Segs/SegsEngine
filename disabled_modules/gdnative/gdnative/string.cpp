/*************************************************************************/
/*  string.cpp                                                           */
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

#include "gdnative/string.h"

#include "core/string_name.h"
#include "core/ustring.h"
#include "core/se_string.h"
#include "core/string_utils.h"
#include "core/variant.h"
#include "core/vector.h"
#include "core/pool_vector.h"
#include "core/array.h"

#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

godot_int GDAPI godot_char_string_length(const godot_char_string *p_cs) {
    const CharString *cs = (const CharString *)p_cs;

    return cs->length();
}

const char GDAPI *godot_char_string_get_data(const godot_char_string *p_cs) {
    const CharString *cs = (const CharString *)p_cs;

    return cs->data();
}

void GDAPI godot_char_string_destroy(godot_char_string *p_cs) {
    CharString *cs = (CharString *)p_cs;

    cs->~CharString();
}

void GDAPI godot_string_new(godot_string *r_dest) {
    se_string *dest = (se_string *)r_dest;
    memnew_placement(dest, se_string);
}

void GDAPI godot_string_new_copy(godot_string *r_dest, const godot_string *p_src) {
    se_string *dest = (se_string *)r_dest;
    const se_string *src = (const se_string *)p_src;
    memnew_placement(dest, se_string(*src));
}

void GDAPI godot_string_new_with_wide_string(godot_string *r_dest, const char16_t *p_contents, const int p_size) {
    String *dest = (String *)r_dest;
    memnew_placement(dest, QString((const QChar *)p_contents, p_size));
}

const char16_t GDAPI *godot_string_operator_index(godot_string *p_self, const godot_int p_idx) {
    String *self = (String *)p_self;
    return (const char16_t *)(self->cdata()+p_idx);
}

char16_t GDAPI godot_string_operator_index_const(const godot_string *p_self, const godot_int p_idx) {
    const String *self = (const String *)p_self;
    return self->cdata()[p_idx].unicode();
}

const char16_t GDAPI *godot_string_wide_str(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    return (const char16_t *)self->cdata();
}

godot_bool GDAPI godot_string_operator_equal(const godot_string *p_self, const godot_string *p_b) {
    const String *self = (const String *)p_self;
    const String *b = (const String *)p_b;
    return *self == *b;
}

godot_bool GDAPI godot_string_operator_less(const godot_string *p_self, const godot_string *p_b) {
    const String *self = (const String *)p_self;
    const String *b = (const String *)p_b;
    return *self < *b;
}

godot_string GDAPI godot_string_operator_plus(const godot_string *p_self, const godot_string *p_b) {
    godot_string ret;
    const String *self = (const String *)p_self;
    const String *b = (const String *)p_b;
    memnew_placement(&ret, String(*self + *b));
    return ret;
}

void GDAPI godot_string_destroy(godot_string *p_self) {
    String *self = (String *)p_self;
    self->~String();
}

/* Standard size stuff */

godot_int GDAPI godot_string_length(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return self->length();
}

/* Helpers */

signed char GDAPI godot_string_casecmp_to(const godot_string *p_self, const godot_string *p_str) {
    const String *self = (const String *)p_self;
    const String *str = (const String *)p_str;

    return StringUtils::compare(*self,*str,StringUtils::CaseSensitive);
}

signed char GDAPI godot_string_nocasecmp_to(const godot_string *p_self, const godot_string *p_str) {
    const String *self = (const String *)p_self;
    const String *str = (const String *)p_str;

    return StringUtils::compare(*self,*str,StringUtils::CaseInsensitive);
}
signed char GDAPI godot_string_naturalnocasecmp_to(const godot_string *p_self, const godot_string *p_str) {
    const String *self = (const String *)p_self;
    const String *str = (const String *)p_str;

    return StringUtils::compare(*self,*str,StringUtils::CaseNatural);
}

godot_bool GDAPI godot_string_begins_with(const godot_string *p_self, const godot_string *p_string) {
    const String *self = (const String *)p_self;
    const String *string = (const String *)p_string;

    return StringUtils::begins_with(*self,*string);
}

godot_bool GDAPI godot_string_begins_with_char_array(const godot_string *p_self, const char *p_char_array) {
    const String *self = (const String *)p_self;

    return StringUtils::begins_with(*self,p_char_array);
}

//godot_array GDAPI godot_string_bigrams(const godot_string *p_self) {
//    const String *self = (const String *)p_self;
//    Vector<String> return_value = self->bigrams();

//    godot_array result;
//    memnew_placement(&result, Array);
//    Array *proxy = (Array *)&result;
//    proxy->resize(return_value.size());
//    for (int i = 0; i < return_value.size(); i++) {
//        (*proxy)[i] = return_value[i];
//    }

//    return result;
//};

godot_string GDAPI godot_string_chr(char16_t p_character) {
    godot_string result;
    memnew_placement(&result, String(p_character));

    return result;
}

godot_bool GDAPI godot_string_ends_with(const godot_string *p_self, const godot_string *p_string) {
    const String *self = (const String *)p_self;
    const String *string = (const String *)p_string;

    return StringUtils::ends_with(*self,*string);
}

godot_int GDAPI godot_string_count(const godot_string *p_self, godot_string p_what, godot_int p_from, godot_int p_to) {
    const String *self = (const String *)p_self;
    String *what = (String *)&p_what;

    return StringUtils::count(*self,*what, p_from, p_to);
}

godot_int GDAPI godot_string_countn(const godot_string *p_self, godot_string p_what, godot_int p_from, godot_int p_to) {
    const String *self = (const String *)p_self;
    String *what = (String *)&p_what;

    return StringUtils::countn(*self,*what, p_from, p_to);
}

godot_int GDAPI godot_string_find(const godot_string *p_self, godot_string p_what) {
    const String *self = (const String *)p_self;
    String *what = (String *)&p_what;

    return StringUtils::find(*self,*what);
}

godot_int GDAPI godot_string_find_from(const godot_string *p_self, godot_string p_what, godot_int p_from) {
    const String *self = (const String *)p_self;
    String *what = (String *)&p_what;

    return StringUtils::find(*self,*what, p_from);
}

godot_int GDAPI godot_string_findmk(const godot_string *p_self, const godot_array *p_keys) {
    const String *self = (const String *)p_self;

    Vector<String> keys;
    Array *keys_proxy = (Array *)p_keys;
    keys.resize(keys_proxy->size());
    for (int i = 0; i < keys_proxy->size(); i++) {
        keys.write[i] = (*keys_proxy)[i];
    }

    return StringUtils::findmk(*self,keys);
}

godot_int GDAPI godot_string_findmk_from(const godot_string *p_self, const godot_array *p_keys, godot_int p_from) {
    const String *self = (const String *)p_self;

    Vector<String> keys;
    Array *keys_proxy = (Array *)p_keys;
    keys.resize(keys_proxy->size());
    for (int i = 0; i < keys_proxy->size(); i++) {
        keys.write[i] = (*keys_proxy)[i];
    }

    return StringUtils::findmk(*self,keys, p_from);
}

godot_int GDAPI godot_string_findmk_from_in_place(const godot_string *p_self, const godot_array *p_keys, godot_int p_from, godot_int *r_key) {
    const String *self = (const String *)p_self;

    Vector<String> keys;
    Array *keys_proxy = (Array *)p_keys;
    keys.resize(keys_proxy->size());
    for (int i = 0; i < keys_proxy->size(); i++) {
        keys.write[i] = (*keys_proxy)[i];
    }

    return StringUtils::findmk(*self,keys, p_from, r_key);
}

godot_int GDAPI godot_string_findn(const godot_string *p_self, godot_string p_what) {
    const String *self = (const String *)p_self;
    String *what = (String *)&p_what;

    return StringUtils::findn(*self,*what);
}

godot_int GDAPI godot_string_findn_from(const godot_string *p_self, godot_string p_what, godot_int p_from) {
    const String *self = (const String *)p_self;
    String *what = (String *)&p_what;

    return StringUtils::findn(*self,*what, p_from);
}

godot_int GDAPI godot_string_find_last(const godot_string *p_self, godot_string p_what) {
    const String *self = (const String *)p_self;
    String *what = (String *)&p_what;

    return StringUtils::find_last(*self,*what);
}

godot_string GDAPI godot_string_format(const godot_string *p_self, const godot_variant *p_values) {
    const String *self = (const String *)p_self;
    const Variant *values = (const Variant *)p_values;
    godot_string result;
    memnew_placement(&result, String(StringUtils::format(*self,*values)));

    return result;
}

godot_string GDAPI godot_string_hex_encode_buffer(const uint8_t *p_buffer, godot_int p_len) {
    godot_string result;
    memnew_placement(&result, String(StringUtils::hex_encode_buffer(p_buffer, p_len)));

    return result;
}

godot_int GDAPI godot_string_hex_to_int(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::hex_to_int(*self);
}

godot_int GDAPI godot_string_hex_to_int_without_prefix(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::hex_to_int(*self,true);
}

godot_string GDAPI godot_string_insert(const godot_string *p_self, godot_int p_at_pos, godot_string p_string) {
    const String *self = (const String *)p_self;
    String *content = (String *)&p_string;
    godot_string result;
    memnew_placement(&result, String(StringUtils::insert(*self,p_at_pos, *content)));

    return result;
}

godot_bool GDAPI godot_string_is_numeric(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::is_numeric(*self);
}

godot_bool GDAPI godot_string_is_subsequence_of(const godot_string *p_self, const godot_string *p_string) {
    const se_string *self = (const se_string *)p_self;
    const se_string *string = (const se_string *)p_string;

    return StringUtils::is_subsequence_of(*self,*string);
}

godot_bool GDAPI godot_string_is_subsequence_ofi(const godot_string *p_self, const godot_string *p_string) {
    const se_string *self = (const se_string *)p_self;
    const se_string *string = (const se_string *)p_string;

    return StringUtils::is_subsequence_of(String(*self),String(*string),StringUtils::CaseInsensitive);
}

godot_string GDAPI godot_string_lpad(const godot_string *p_self, godot_int p_min_length) {
    const se_string *self = (const se_string *)p_self;
    godot_string result;
    memnew_placement(&result, se_string(StringUtils::lpad(*self,p_min_length)));

    return result;
}

godot_string GDAPI godot_string_lpad_with_custom_character(const godot_string *p_self, godot_int p_min_length, const godot_string *p_character) {
    const se_string *self = (const se_string *)p_self;
    const se_string *character = (const se_string *)p_character;
    godot_string result;
    memnew_placement(&result, se_string(StringUtils::lpad(*self,p_min_length, character->front())));

    return result;
}

godot_bool GDAPI godot_string_match(const godot_string *p_self, const godot_string *p_wildcard) {
    const se_string *self = (const se_string *)p_self;
    const se_string *wildcard = (const se_string *)p_wildcard;

    return StringUtils::match(*self,*wildcard);
}

godot_bool GDAPI godot_string_matchn(const godot_string *p_self, const godot_string *p_wildcard) {
    const se_string *self = (const se_string *)p_self;
    const se_string *wildcard = (const se_string *)p_wildcard;

    return StringUtils::match(*self,*wildcard,StringUtils::CaseInsensitive);
}

godot_string GDAPI godot_string_md5(const uint8_t *p_md5) {
    godot_string result;
    memnew_placement(&result, String(StringUtils::md5(p_md5)));

    return result;
}

godot_string GDAPI godot_string_num(double p_num) {
    godot_string result;
    memnew_placement(&result, String(StringUtils::num(p_num)));

    return result;
}

godot_string GDAPI godot_string_num_int64(int64_t p_num, godot_int p_base) {
    godot_string result;
    memnew_placement(&result, String(StringUtils::num_int64(p_num, p_base)));

    return result;
}

godot_string GDAPI godot_string_num_int64_capitalized(int64_t p_num, godot_int p_base, godot_bool p_capitalize_hex) {
    godot_string result;
    memnew_placement(&result, String(StringUtils::num_int64(p_num, p_base, true)));

    return result;
}

godot_string GDAPI godot_string_num_real(double p_num) {
    godot_string result;
    memnew_placement(&result, String(StringUtils::num_real(p_num)));

    return result;
}

godot_string GDAPI godot_string_num_scientific(double p_num) {
    godot_string result;
    memnew_placement(&result, String(StringUtils::num_scientific(p_num)));

    return result;
}

godot_string GDAPI godot_string_num_with_decimals(double p_num, godot_int p_decimals) {
    godot_string result;
    memnew_placement(&result, String(StringUtils::num(p_num, p_decimals)));

    return result;
}

godot_string GDAPI godot_string_pad_decimals(const godot_string *p_self, godot_int p_digits) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::pad_decimals(*self,p_digits)));

    return result;
}

godot_string GDAPI godot_string_pad_zeros(const godot_string *p_self, godot_int p_digits) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::pad_zeros(*self,p_digits)));

    return result;
}

godot_string GDAPI godot_string_replace(const godot_string *p_self, godot_string p_key, godot_string p_with) {
    const String *self = (const String *)p_self;
    String *key = (String *)&p_key;
    String *with = (String *)&p_with;
    godot_string result;
    memnew_placement(&result, String(StringUtils::replace(*self,*key, *with)));

    return result;
}

godot_string GDAPI godot_string_replacen(const godot_string *p_self, godot_string p_key, godot_string p_with) {
    const String *self = (const String *)p_self;
    String *key = (String *)&p_key;
    String *with = (String *)&p_with;
    godot_string result;
    memnew_placement(&result, String(StringUtils::replacen(*self,*key, *with)));

    return result;
}

godot_int GDAPI godot_string_rfind(const godot_string *p_self, godot_string p_what) {
    const String *self = (const String *)p_self;
    String *what = (String *)&p_what;

    return StringUtils::rfind(*self,*what);
}

godot_int GDAPI godot_string_rfindn(const godot_string *p_self, godot_string p_what) {
    const String *self = (const String *)p_self;
    String *what = (String *)&p_what;

    return StringUtils::rfindn(*self,*what);
}

godot_int GDAPI godot_string_rfind_from(const godot_string *p_self, godot_string p_what, godot_int p_from) {
    const String *self = (const String *)p_self;
    String *what = (String *)&p_what;

    return StringUtils::rfind(*self,*what, p_from);
}

godot_int GDAPI godot_string_rfindn_from(const godot_string *p_self, godot_string p_what, godot_int p_from) {
    const String *self = (const String *)p_self;
    String *what = (String *)&p_what;

    return StringUtils::rfindn(*self,*what, p_from);
}

godot_string GDAPI godot_string_replace_first(const godot_string *p_self, godot_string p_key, godot_string p_with) {
    const String *self = (const String *)p_self;
    String *key = (String *)&p_key;
    String *with = (String *)&p_with;
    godot_string result;
    memnew_placement(&result, String(StringUtils::replace_first(*self,*key, *with)));

    return result;
}

godot_string GDAPI godot_string_rpad(const godot_string *p_self, godot_int p_min_length) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::rpad(*self,p_min_length)));

    return result;
}

godot_string GDAPI godot_string_rpad_with_custom_character(const godot_string *p_self, godot_int p_min_length, const godot_string *p_character) {
    const se_string *self = (const se_string *)p_self;
    const se_string *character = (const se_string *)p_character;
    godot_string result;
    memnew_placement(&result, se_string(StringUtils::rpad(*self,p_min_length, character->front())));

    return result;
}

//godot_real GDAPI godot_string_similarity(const godot_string *p_self, const godot_string *p_string) {
//    const String *self = (const String *)p_self;
//    const String *string = (const String *)p_string;

//    return self->similarity(*string);
//}

godot_string GDAPI godot_string_sprintf(const godot_string *p_self, const godot_array *p_values, godot_bool *p_error) {
    const String *self = (const String *)p_self;
    const Array *values = (const Array *)p_values;

    godot_string result;
    String return_value = StringUtils::sprintf(*self,*values, p_error);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_substr(const godot_string *p_self, godot_int p_from, godot_int p_chars) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::substr(*self,p_from, p_chars)));

    return result;
}

double GDAPI godot_string_to_double(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::to_double(*self);
}

godot_real GDAPI godot_string_to_float(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::to_float(*self);
}

godot_int GDAPI godot_string_to_int(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::to_int(*self);
}

godot_string GDAPI godot_string_capitalize(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::capitalize(*self)));

    return result;
}

godot_string GDAPI godot_string_camelcase_to_underscore(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::camelcase_to_underscore(*self,false)));

    return result;
}

godot_string GDAPI godot_string_camelcase_to_underscore_lowercased(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::camelcase_to_underscore(*self)));

    return result;
}

double GDAPI godot_string_char_to_double(const char *p_what) {
    return StringUtils::to_double(p_what);
}

godot_int GDAPI godot_string_char_to_int(const char *p_what) {
    return StringUtils::to_int(p_what);
}

int64_t GDAPI godot_string_wchar_to_int(const char16_t *p_str) {
    return StringUtils::to_int((const QChar *)p_str);
}

godot_int GDAPI godot_string_char_to_int_with_len(const char *p_what, godot_int p_len) {
    return StringUtils::to_int(p_what, p_len);
}

int64_t GDAPI godot_string_char_to_int64_with_len(const char16_t *p_str, int p_len) {
    return StringUtils::to_int((const QChar *)p_str, p_len);
}

int64_t GDAPI godot_string_hex_to_int64(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::hex_to_int64(*self,false);
}

int64_t GDAPI godot_string_hex_to_int64_with_prefix(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::hex_to_int64(*self);
}

int64_t GDAPI godot_string_to_int64(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::to_int64(*self);
}

//double GDAPI godot_string_unicode_char_to_double(const char16_t *p_str, const char16_t **r_end) {
//    return StringUtils::to_double((const QChar *)p_str, (const QChar **)r_end);
//}

godot_string GDAPI godot_string_get_slice(const godot_string *p_self, godot_string p_splitter, godot_int p_slice) {
    const String *self = (const String *)p_self;
    String *splitter = (String *)&p_splitter;
    godot_string result;
    memnew_placement(&result, String(StringUtils::get_slice(*self,*splitter, p_slice)));

    return result;
}

godot_string GDAPI godot_string_get_slicec(const godot_string *p_self, char16_t p_splitter, godot_int p_slice) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::get_slice(*self,p_splitter, p_slice)));

    return result;
}

godot_array GDAPI godot_string_split(const godot_string *p_self, const godot_string *p_splitter) {
    const String *self = (const String *)p_self;
    const String *splitter = (const String *)p_splitter;
    godot_array result;
    memnew_placement(&result, Array);
    Array *proxy = (Array *)&result;
    Vector<String> return_value = StringUtils::split(*self,*splitter, false);

    proxy->resize(return_value.size());
    for (int i = 0; i < return_value.size(); i++) {
        (*proxy)[i] = return_value[i];
    }

    return result;
}

godot_array GDAPI godot_string_split_allow_empty(const godot_string *p_self, const godot_string *p_splitter) {
    const String *self = (const String *)p_self;
    const String *splitter = (const String *)p_splitter;
    godot_array result;
    memnew_placement(&result, Array);
    Array *proxy = (Array *)&result;
    Vector<String> return_value = StringUtils::split(*self,*splitter);

    proxy->resize(return_value.size());
    for (int i = 0; i < return_value.size(); i++) {
        (*proxy)[i] = return_value[i];
    }

    return result;
}

godot_array GDAPI godot_string_split_floats(const godot_string *p_self, const godot_string *p_splitter) {
    const String *self = (const String *)p_self;
    const String *splitter = (const String *)p_splitter;
    godot_array result;
    memnew_placement(&result, Array);
    Array *proxy = (Array *)&result;
    Vector<float> return_value = StringUtils::split_floats(*self,*splitter, false);

    proxy->resize(return_value.size());
    for (int i = 0; i < return_value.size(); i++) {
        (*proxy)[i] = return_value[i];
    }

    return result;
}

godot_array GDAPI godot_string_split_floats_allows_empty(const godot_string *p_self, const godot_string *p_splitter) {
    const String *self = (const String *)p_self;
    const String *splitter = (const String *)p_splitter;
    godot_array result;
    memnew_placement(&result, Array);
    Array *proxy = (Array *)&result;
    Vector<float> return_value = StringUtils::split_floats(*self,*splitter);

    proxy->resize(return_value.size());
    for (int i = 0; i < return_value.size(); i++) {
        (*proxy)[i] = return_value[i];
    }

    return result;
}

godot_array GDAPI godot_string_split_floats_mk(const godot_string *p_self, const godot_array *p_splitters) {
    const se_string *self = (const se_string *)p_self;

    se_string splitters;
    Array *splitter_proxy = (Array *)p_splitters;
    splitters.resize(splitter_proxy->size());
    for (int i = 0; i < splitter_proxy->size(); i++) {
        splitters.write[i] = (*splitter_proxy)[i];
    }

    godot_array result;
    memnew_placement(&result, Array);
    Array *proxy = (Array *)&result;
    Vector<float> return_value = StringUtils::split_floats_mk(*self,splitters, false);

    proxy->resize(return_value.size());
    for (int i = 0; i < return_value.size(); i++) {
        (*proxy)[i] = return_value[i];
    }

    return result;
}

godot_array GDAPI godot_string_split_floats_mk_allows_empty(const godot_string *p_self, const godot_array *p_splitters) {
    const se_string *self = (const se_string *)p_self;

    Vector<String> splitters;
    Array *splitter_proxy = (Array *)p_splitters;
    splitters.resize(splitter_proxy->size());
    for (int i = 0; i < splitter_proxy->size(); i++) {
        splitters.write[i] = (*splitter_proxy)[i];
    }

    godot_array result;
    memnew_placement(&result, Array);
    Array *proxy = (Array *)&result;
    Vector<float> return_value = StringUtils::split_floats_mk(*self,splitters);

    proxy->resize(return_value.size());
    for (int i = 0; i < return_value.size(); i++) {
        (*proxy)[i] = return_value[i];
    }

    return result;
}

godot_array GDAPI godot_string_split_spaces(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_array result;
    memnew_placement(&result, Array);
    Array *proxy = (Array *)&result;
    Vector<String> return_value = StringUtils::split_spaces(*self);

    proxy->resize(return_value.size());
    for (int i = 0; i < return_value.size(); i++) {
        (*proxy)[i] = return_value[i];
    }

    return result;
}

godot_int GDAPI godot_string_get_slice_count(const godot_string *p_self, godot_string p_splitter) {
    const String *self = (const String *)p_self;
    String *splitter = (String *)&p_splitter;

    return StringUtils::get_slice_count(*self,*splitter);
}

char16_t GDAPI godot_string_char_lowercase(char16_t p_char) {
    return CharType(p_char).toLower().unicode();
}

char16_t GDAPI godot_string_char_uppercase(char16_t p_char) {
    return CharType(p_char).toUpper().unicode();
}

godot_string GDAPI godot_string_to_lower(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::to_lower(*self)));

    return result;
}

godot_string GDAPI godot_string_to_upper(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::to_upper(*self)));

    return result;
}

godot_string GDAPI godot_string_get_basename(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(PathUtils::get_basename(*self)));

    return result;
}

godot_string GDAPI godot_string_get_extension(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(PathUtils::get_extension(*self)));

    return result;
}

godot_string GDAPI godot_string_left(const godot_string *p_self, godot_int p_pos) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::left(*self,p_pos)));

    return result;
}

char16_t GDAPI godot_string_ord_at(const godot_string *p_self, godot_int p_idx) {
    const String *self = (const String *)p_self;

    return StringUtils::ord_at(*self,p_idx).unicode();
}

godot_string GDAPI godot_string_plus_file(const godot_string *p_self, const godot_string *p_file) {
    const String *self = (const String *)p_self;
    const String *file = (const String *)p_file;
    godot_string result;
    memnew_placement(&result, String(PathUtils::plus_file(*self,*file)));

    return result;
}

godot_string GDAPI godot_string_right(const godot_string *p_self, godot_int p_pos) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::right(*self,p_pos)));

    return result;
}

godot_string GDAPI godot_string_strip_edges(const godot_string *p_self, godot_bool p_left, godot_bool p_right) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::strip_edges(*self,p_left, p_right)));

    return result;
}

godot_string GDAPI godot_string_strip_escapes(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::strip_escapes(*self)));

    return result;
}

void GDAPI godot_string_erase(godot_string *p_self, godot_int p_pos, godot_int p_chars) {
    String *self = (String *)p_self;

    return StringUtils::erase(*self,p_pos, p_chars);
}

godot_char_string GDAPI godot_string_ascii(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_char_string result;

    memnew_placement(&result, CharString(StringUtils::ascii(*self)));

    return result;
}

godot_char_string GDAPI godot_string_ascii_extended(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    godot_char_string result;

    memnew_placement(&result, CharString(StringUtils::ascii(*self,true)));

    return result;
}

godot_char_string GDAPI godot_string_utf8(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    godot_char_string result;

    memnew_placement(&result, CharString(StringUtils::to_utf8(*self)));

    return result;
}

godot_bool GDAPI godot_string_parse_utf8(godot_string *p_self, const char *p_utf8) {
    String *self = (String *)p_self;
    *self = StringUtils::from_utf8(p_utf8);
    return !self->empty();
}

godot_bool GDAPI godot_string_parse_utf8_with_len(godot_string *p_self, const char *p_utf8, godot_int p_len) {
    String *self = (String *)p_self;

    *self = StringUtils::from_utf8(p_utf8,p_len);
    return !self->empty();
}

godot_string GDAPI godot_string_chars_to_utf8(const char *p_utf8) {
    godot_string result;
    memnew_placement(&result, String(StringUtils::from_utf8(p_utf8)));

    return result;
}

godot_string GDAPI godot_string_chars_to_utf8_with_len(const char *p_utf8, godot_int p_len) {
    godot_string result;
    memnew_placement(&result, String(StringUtils::from_utf8(p_utf8, p_len)));

    return result;
}

uint32_t GDAPI godot_string_hash(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::hash(*self);
}

uint64_t GDAPI godot_string_hash64(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::hash64(*self);
}

uint32_t GDAPI godot_string_hash_chars(const char *p_cstr) {
    return StringUtils::hash(p_cstr);
}

uint32_t GDAPI godot_string_hash_chars_with_len(const char *p_cstr, godot_int p_len) {
    return StringUtils::hash(p_cstr, p_len);
}

uint32_t GDAPI godot_string_hash_utf8_chars(const char16_t *p_str) {
    return StringUtils::hash((const QChar *)p_str);
}

uint32_t GDAPI godot_string_hash_utf8_chars_with_len(const char16_t *p_str, godot_int p_len) {
    return StringUtils::hash((const QChar *)p_str, p_len);
}

godot_pool_byte_array GDAPI godot_string_md5_buffer(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    Vector<uint8_t> tmp_result = StringUtils::md5_buffer(*self);

    godot_pool_byte_array result;
    memnew_placement(&result, PoolByteArray);
    PoolByteArray *proxy = (PoolByteArray *)&result;
    PoolByteArray::Write proxy_writer = proxy->write();
    proxy->resize(tmp_result.size());

    for (int i = 0; i < tmp_result.size(); i++) {
        proxy_writer[i] = tmp_result[i];
    }

    return result;
}

godot_string GDAPI godot_string_md5_text(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::md5_text(*self)));

    return result;
}

godot_pool_byte_array GDAPI godot_string_sha256_buffer(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    Vector<uint8_t> tmp_result = StringUtils::sha256_buffer(*self);

    godot_pool_byte_array result;
    memnew_placement(&result, PoolByteArray);
    PoolByteArray *proxy = (PoolByteArray *)&result;
    PoolByteArray::Write proxy_writer = proxy->write();
    proxy->resize(tmp_result.size());

    for (int i = 0; i < tmp_result.size(); i++) {
        proxy_writer[i] = tmp_result[i];
    }

    return result;
}

godot_string GDAPI godot_string_sha256_text(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    memnew_placement(&result, String(StringUtils::sha256_text(*self)));

    return result;
}

godot_bool godot_string_empty(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return self->empty();
}

// path functions
godot_string GDAPI godot_string_get_base_dir(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = PathUtils::get_base_dir(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_get_file(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = PathUtils::get_file(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_humanize_size(size_t p_size) {
    godot_string result;
    String return_value = PathUtils::humanize_size(p_size);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_bool GDAPI godot_string_is_abs_path(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return PathUtils::is_abs_path(*self);
}

godot_bool GDAPI godot_string_is_rel_path(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return PathUtils::is_rel_path(*self);
}

godot_bool GDAPI godot_string_is_resource_file(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return PathUtils::is_resource_file(*self);
}

godot_string GDAPI godot_string_path_to(const godot_string *p_self, const godot_string *p_path) {
    const String *self = (const String *)p_self;
    String *path = (String *)p_path;
    godot_string result;
    String return_value = PathUtils::path_to(*self,*path);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_path_to_file(const godot_string *p_self, const godot_string *p_path) {
    const String *self = (const String *)p_self;
    String *path = (String *)p_path;
    godot_string result;
    String return_value = PathUtils::path_to_file(*self,*path);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_simplify_path(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = PathUtils::simplify_path(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_c_escape(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = StringUtils::c_escape(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_c_escape_multiline(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = StringUtils::c_escape_multiline(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_c_unescape(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = StringUtils::c_unescape(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_http_escape(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = StringUtils::http_escape(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_http_unescape(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = StringUtils::http_unescape(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_json_escape(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = StringUtils::json_escape(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_word_wrap(const godot_string *p_self, godot_int p_chars_per_line) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = StringUtils::word_wrap(*self,p_chars_per_line);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_xml_escape(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = StringUtils::xml_escape(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_xml_escape_with_quotes(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = StringUtils::xml_escape(*self,true);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_xml_unescape(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = StringUtils::xml_unescape(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_percent_decode(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = StringUtils::percent_decode(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_percent_encode(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = StringUtils::percent_encode(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_bool GDAPI godot_string_is_valid_float(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::is_valid_float(*self);
}

godot_bool GDAPI godot_string_is_valid_hex_number(const godot_string *p_self, godot_bool p_with_prefix) {
    const String *self = (const String *)p_self;

    return StringUtils::is_valid_hex_number(StringUtils::utf8(*self).data(),p_with_prefix);
}

godot_bool GDAPI godot_string_is_valid_html_color(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::is_valid_html_color(*self);
}

godot_bool GDAPI godot_string_is_valid_identifier(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::is_valid_identifier(*self);
}

godot_bool GDAPI godot_string_is_valid_integer(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::is_valid_integer(*self);
}

godot_bool GDAPI godot_string_is_valid_ip_address(const godot_string *p_self) {
    const String *self = (const String *)p_self;

    return StringUtils::is_valid_ip_address(StringUtils::utf8(*self).data());
}

godot_string GDAPI godot_string_dedent(const godot_string *p_self) {
    const String *self = (const String *)p_self;
    godot_string result;
    String return_value = StringUtils::dedent(*self);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_trim_prefix(const godot_string *p_self, const godot_string *p_prefix) {
    const String *self = (const String *)p_self;
    String *prefix = (String *)p_prefix;
    godot_string result;
    String return_value = StringUtils::trim_prefix(*self,*prefix);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_trim_suffix(const godot_string *p_self, const godot_string *p_suffix) {
    const String *self = (const String *)p_self;
    String *suffix = (String *)p_suffix;
    godot_string result;
    String return_value = StringUtils::trim_suffix(*self,*suffix);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_string GDAPI godot_string_rstrip(const godot_string *p_self, const godot_string *p_chars) {
    const String *self = (const String *)p_self;
    String *chars = (String *)p_chars;
    godot_string result;
    String return_value = StringUtils::rstrip(*self,*chars);
    memnew_placement(&result, String(return_value));

    return result;
}

godot_pool_string_array GDAPI godot_string_rsplit(const godot_string *p_self, const godot_string *p_divisor,
        const godot_bool p_allow_empty, const godot_int p_maxsplit) {
    const String *self = (const String *)p_self;
    String *divisor = (String *)p_divisor;

    godot_pool_string_array result;
    memnew_placement(&result, PoolStringArray);
    PoolStringArray *proxy = (PoolStringArray *)&result;
    PoolStringArray::Write proxy_writer = proxy->write();
    Vector<String> tmp_result = StringUtils::rsplit(*self,*divisor, p_allow_empty, p_maxsplit);
    proxy->resize(tmp_result.size());

    for (int i = 0; i < tmp_result.size(); i++) {
        proxy_writer[i] = tmp_result[i];
    }

    return result;
}

#ifdef __cplusplus
}
#endif
