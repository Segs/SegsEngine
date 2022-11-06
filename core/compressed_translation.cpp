/*************************************************************************/
/*  compressed_translation.cpp                                           */
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

#include "compressed_translation.h"

#include "object_tooling.h"
#include "core/method_bind.h"
#include "io/compression.h"

void PHashTranslation::generate(const Ref<Translation> &p_from) {
    Tooling::generate_phash_translation(*this, p_from);
}

bool PHashTranslation::_set(const StringName &p_name, const Variant &p_value) {

    if (p_name == "hash_table") {
        hash_table = p_value.as<Vector<int>>();
    } else if (p_name == "bucket_table") {
        bucket_table = p_value.as<Vector<int>>();
    } else if (p_name == "strings") {
        strings = p_value.as<Vector<uint8_t>>();
    } else if (p_name == "load_from") {
        generate(refFromVariant<Translation>(p_value));
    } else {
        return false;
    }

    return true;
}

bool PHashTranslation::_get(const StringName &p_name, Variant &r_ret) const {

    if (p_name == "hash_table") {
        r_ret = hash_table;
    } else if (p_name == "bucket_table") {
        r_ret = bucket_table;
    } else if (p_name == "strings") {
        r_ret = strings;
    } else {
        return false;
    }

    return true;
}

StringName PHashTranslation::get_message(const StringName &p_src_text) const {
    const size_t htsize = hash_table.size();

    if (htsize == 0) {
        return StringName();
    }

    uint32_t h = phash_calculate(0, p_src_text.asCString());

    const uint32_t *htptr = (const uint32_t *)hash_table.data();
    const uint32_t *btptr = (const uint32_t *)bucket_table.data();
    const char *sptr = (const char *)strings.data();

    uint32_t p = htptr[h % htsize];

    if (p == 0xFFFFFFFF) {
        return StringName(); //nothing
    }

    const Bucket &bucket = *(const Bucket *)&btptr[p];

    h = phash_calculate(bucket.func, p_src_text.asCString());

    int idx = -1;

    for (int i = 0; i < bucket.size; i++) {

        if (bucket.elem[i].key == h) {

            idx = i;
            break;
        }
    }

    if (idx == -1) {
        return StringName();
    }

    String rstr;
    if (bucket.elem[idx].comp_size == bucket.elem[idx].uncomp_size) {

        rstr = String(&sptr[bucket.elem[idx].str_offset], bucket.elem[idx].uncomp_size);

    } else {

        rstr.resize(bucket.elem[idx].uncomp_size + 1);
        Compression::decompress_short_string(&sptr[bucket.elem[idx].str_offset], bucket.elem[idx].comp_size, rstr.data(),
                bucket.elem[idx].uncomp_size);
    }
    return StringName(rstr);
}

void PHashTranslation::_get_property_list(Vector<PropertyInfo> *p_list) const {

    p_list->push_back(PropertyInfo(VariantType::POOL_INT_ARRAY, "hash_table"));
    p_list->push_back(PropertyInfo(VariantType::POOL_INT_ARRAY, "bucket_table"));
    p_list->push_back(PropertyInfo(VariantType::POOL_BYTE_ARRAY, "strings"));
    p_list->push_back(PropertyInfo(
            VariantType::OBJECT, "load_from", PropertyHint::ResourceType, "Translation", PROPERTY_USAGE_EDITOR));
}

IMPL_GDCLASS(PHashTranslation)

void PHashTranslation::_bind_methods() {

    SE_BIND_METHOD(PHashTranslation,generate);
}

PHashTranslation::PHashTranslation() = default;
