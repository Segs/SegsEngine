/*************************************************************************/
/*  file_access.cpp                                                      */
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

#include "file_access.h"

#include "core/crypto/crypto_core.h"
#include "core/io/file_access_pack.h"
#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "core/pool_vector.h"
#include "core/project_settings.h"
#include "core/string.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"

FileAccess::CreateFunc FileAccess::create_func[ACCESS_MAX] = { nullptr, nullptr };

FileAccess::FileCloseFailNotify FileAccess::close_fail_notify = nullptr;

bool FileAccess::backup_save = false;

FileAccess *FileAccess::create(AccessType p_access) {

    ERR_FAIL_INDEX_V(p_access, ACCESS_MAX, nullptr);

    FileAccess *ret = create_func[p_access]();
    ret->_set_access_type(p_access);
    return ret;
}

bool FileAccess::exists(StringView p_name) {

    auto *packed_data = PackedData::get_singleton();
    if (packed_data && !packed_data->is_disabled() && packed_data->has_path(p_name))
        return true;

    FileAccess *f = open(p_name, READ);
    if (!f)
        return false;
    memdelete(f);
    return true;
}

void FileAccess::_set_access_type(AccessType p_access) {

    _access_type = p_access;
}

const String &FileAccess::get_path() const { return null_string; }

const String &FileAccess::get_path_absolute() const { return null_string; }

FileAccess *FileAccess::create_for_path(StringView p_path) {

    FileAccess *ret = nullptr;
    if (StringUtils::begins_with(p_path,"res://")) {

        ret = create(ACCESS_RESOURCES);
    } else if (StringUtils::begins_with(p_path,"user://")) {

        ret = create(ACCESS_USERDATA);

    } else {

        ret = create(ACCESS_FILESYSTEM);
    }

    return ret;
}

Error FileAccess::reopen(StringView p_path, int p_mode_flags) {

    return _open(p_path, p_mode_flags);
}

FileAccess *FileAccess::open(StringView p_path, int p_mode_flags, Error *r_error) {

    //try packed data first

    FileAccess *ret = nullptr;
    if (!(p_mode_flags & WRITE) && PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled()) {
        ret = PackedData::get_singleton()->try_open_path(p_path);
        if (ret) {
            if (r_error)
                *r_error = OK;
            return ret;
        }
    }

    ret = create_for_path(p_path);
    Error err = ret->_open(p_path, p_mode_flags);

    if (r_error)
        *r_error = err;
    if (err != OK) {

        memdelete(ret);
        ret = nullptr;
    }

    return ret;
}

FileAccess::CreateFunc FileAccess::get_create_func(AccessType p_access) {

    return create_func[p_access];
}

String FileAccess::fix_path(StringView p_path) const {
    //helper used by file accesses that use a single filesystem

    String r_path(PathUtils::from_native_path(p_path));

    switch (_access_type) {

        case ACCESS_RESOURCES: {

            if (ProjectSettings::get_singleton()) {
                if (StringUtils::begins_with(r_path,"res://")) {

                    String resource_path = ProjectSettings::get_singleton()->get_resource_path();
                    if (!resource_path.empty()) {

                        return StringUtils::replace(r_path,"res:/", resource_path);
                    }
                    return StringUtils::replace(r_path,"res://", "");
                }
            }

        } break;
        case ACCESS_USERDATA: {

            if (StringUtils::begins_with(r_path,"user://")) {

                String data_dir = OS::get_singleton()->get_user_data_dir();
                if (not data_dir.empty()) {

                    return StringUtils::replace(r_path,"user:/", data_dir);
                }
                return StringUtils::replace(r_path,"user://", "");
            }

        } break;
        case ACCESS_FILESYSTEM: {

            return r_path;
        }
        case ACCESS_MAX: break; // Can't happen, but silences warning
    }

    return r_path;
}

/* these are all implemented for ease of porting, then can later be optimized */

uint16_t FileAccess::get_16() const {

    uint16_t res;
    uint8_t a, b;

    a = get_8();
    b = get_8();

    if (endian_swap) {

        SWAP(a, b);
    }

    res = b;
    res <<= 8;
    res |= a;

    return res;
}
uint32_t FileAccess::get_32() const {

    uint32_t res;
    uint16_t a, b;

    a = get_16();
    b = get_16();

    if (endian_swap) {

        SWAP(a, b);
    }

    res = b;
    res <<= 16;
    res |= a;

    return res;
}
uint64_t FileAccess::get_64() const {

    uint64_t res;
    uint32_t a, b;

    a = get_32();
    b = get_32();

    if (endian_swap) {

        SWAP(a, b);
    }

    res = b;
    res <<= 32;
    res |= a;

    return res;
}

float FileAccess::get_float() const {

    MarshallFloat m;
    m.i = get_32();
    return m.f;
}

real_t FileAccess::get_real() const {

    if (real_is_double)
        return get_double();
    else
        return get_float();
}

double FileAccess::get_double() const {

    MarshallDouble m;
    m.l = get_64();
    return m.d;
}

String FileAccess::get_token() const {

    String token;

    uint8_t c = get_8();

    while (!eof_reached()) {

        if (c <= ' ') {
            if (token.length())
                break;
        } else {
            token.push_back(c);
        }
        c = get_8();
    }

    return token;
}

String FileAccess::get_line() const {

    String line;

    uint8_t c = get_8();

    while (!eof_reached()) {

        if (c == '\n' || c == '\0') {
            return line;
        } else if (c != '\r')
            line.push_back(c);

        c = get_8();
    }
    return line;
}

Vector<String> FileAccess::get_csv_line(char p_delim) const {
    ERR_FAIL_COND_V_MSG(p_delim == '"', Vector<String>(), "The double quotation mark character (\") is not supported as a delimiter for CSV lines.");

    String line;

    // CSV can support entries with line breaks as long as they are enclosed
    // in double quotes. So our "line" might be more than a single line in the
    // text file.
    int qc;
    do {
        if (eof_reached())
            break;

        line += get_line() + "\n";
        qc = 0;
        for (size_t i = 0; i < line.length(); i++) {

            if (line[i] == '"')
                qc++;
        }

    } while (qc % 2);

    // Remove the extraneous newline we've added above.
    line = StringUtils::substr(line,0, line.length() - 1);

    Vector<String> strings;

    bool in_quote = false;
    String current;
    for (size_t i = 0; i < line.length(); i++) {

        char c = line[i];
        // A delimiter ends the current entry, unless it's in a quoted string.
        if (!in_quote && c == p_delim) {
            strings.emplace_back(eastl::move(current));
            current = {};
        } else if (c == '"') {
            // Doubled quotes are escapes for intentional quotes in the string.
            if (line[i + 1] == '"' && in_quote ) {
                current += '"';
                i++;
            } else {

                in_quote = !in_quote;
            }
        } else {
            current += c;
        }
    }

    strings.emplace_back(eastl::move(current));

    return strings;
}

uint64_t FileAccess::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
    ERR_FAIL_COND_V(!p_dst && p_length > 0, -1);

    uint64_t i = 0;
    for (i = 0; i < p_length && !eof_reached(); i++)
        p_dst[i] = get_8();

    return i;
}

String FileAccess::get_as_utf8_string() const {
    String s;
    uint64_t len = get_len();
    s.resize(len + 1);

    uint64_t r = get_buffer((uint8_t *)s.data(), len);
    ERR_FAIL_COND_V(r != len, String());
    s[len] = 0;
    return s;
}

void FileAccess::store_16(uint16_t p_dest) {

    uint8_t a, b;

    a = p_dest & 0xFF;
    b = p_dest >> 8;

    if (endian_swap) {

        SWAP(a, b);
    }

    store_8(a);
    store_8(b);
}
void FileAccess::store_32(uint32_t p_dest) {

    uint16_t a, b;

    a = p_dest & 0xFFFF;
    b = p_dest >> 16;

    if (endian_swap) {

        SWAP(a, b);
    }

    store_16(a);
    store_16(b);
}
void FileAccess::store_64(uint64_t p_dest) {

    uint32_t a, b;

    a = p_dest & 0xFFFFFFFF;
    b = p_dest >> 32;

    if (endian_swap) {

        SWAP(a, b);
    }

    store_32(a);
    store_32(b);
}

void FileAccess::store_real(real_t p_real) {

    if (sizeof(real_t) == 4)
        store_float(p_real);
    else
        store_double(p_real);
}

void FileAccess::store_float(float p_dest) {

    MarshallFloat m;
    m.f = p_dest;
    store_32(m.i);
}

void FileAccess::store_double(double p_dest) {

    MarshallDouble m;
    m.d = p_dest;
    store_64(m.l);
}

uint64_t FileAccess::get_modified_time(StringView p_file) {

    PackedData *pd=PackedData::get_singleton();
    if (pd && !pd->is_disabled() && (pd->has_path(p_file) || pd->has_directory(p_file)))
        return 0;

    FileAccess *fa = create_for_path(p_file);
    ERR_FAIL_COND_V_MSG(!fa, 0, "Cannot create FileAccess for path '" + String(p_file) + "'.");

    uint64_t mt = fa->_get_modified_time(p_file);
    memdelete(fa);
    return mt;
}

uint32_t FileAccess::get_unix_permissions(StringView p_file) {

    PackedData *pd=PackedData::get_singleton();
    if (pd && !pd->is_disabled() && (pd->has_path(p_file)|| pd->has_directory(p_file)))
        return 0;

    FileAccess *fa = create_for_path(p_file);
    ERR_FAIL_COND_V_MSG(!fa, 0, "Cannot create FileAccess for path '" + String(p_file) + "'.");

    uint32_t mt = fa->_get_unix_permissions(p_file);
    memdelete(fa);
    return mt;
}

Error FileAccess::set_unix_permissions(StringView p_file, uint32_t p_permissions) {
    auto packed_data = PackedData::get_singleton();
    if (packed_data && !packed_data->is_disabled() && (packed_data->has_path(p_file) || packed_data->has_directory(p_file)))
        return ERR_UNAVAILABLE;

    FileAccess *fa = create_for_path(p_file);
    ERR_FAIL_COND_V_MSG(!fa, ERR_CANT_CREATE, "Cannot create FileAccess for path '" + String(p_file) + "'.");

    Error err = fa->_set_unix_permissions(p_file, p_permissions);
    memdelete(fa);
    return err;
}

void FileAccess::store_string(StringView p_string) {

    if (p_string.empty())
        return;

    store_buffer((const uint8_t *)p_string.data(), p_string.length());
}

void FileAccess::store_pascal_string(StringView p_string) {

    store_32(p_string.length());
    store_buffer((const uint8_t *)p_string.data(), p_string.length());
}

String FileAccess::get_pascal_string() {

    uint32_t sl = get_32();
    String cs;
    cs.resize(sl);
    get_buffer((uint8_t *)cs.data(), sl);
    return cs;
}

void FileAccess::store_line(StringView p_line) {

    store_string(p_line);
    store_8('\n');
}

void FileAccess::store_csv_line(const Vector<String> &p_values, char p_delim) {

    String line;
    int size = p_values.size();
    for (int i = 0; i < size; ++i) {
        String value = p_values[i];

        if (StringUtils::contains(value,'"') || StringUtils::contains(value,p_delim) || StringUtils::contains(value,'\n')) {
            value = "\"" + StringUtils::replace(value,"\"", "\"\"") + "\"";
        }
        if (i < size - 1) {
            value += p_delim;
        }

        line += value;
    }

    store_line(line);
}

void FileAccess::store_buffer(const uint8_t *p_src, uint64_t p_length) {

    for (uint64_t i = 0; i < p_length; i++)
        store_8(p_src[i]);
}

Vector<uint8_t> FileAccess::get_file_as_array(StringView p_path, Error *r_error) {

    FileAccess *f = FileAccess::open(p_path, READ, r_error);
    if (!f) {
        if (r_error) { // if error requested, do not throw error
            return Vector<uint8_t>();
        }
        ERR_FAIL_V_MSG(Vector<uint8_t>(), "Can't open file from path '" + String(p_path) + "'.");
    }
    Vector<uint8_t> data;
    data.resize(f->get_len());
    f->get_buffer(data.data(), data.size());
    memdelete(f);
    return data;
}

String FileAccess::get_file_as_string(StringView p_path, Error *r_error) {

    FileAccess *f = FileAccess::open(p_path, READ, r_error);
    if (!f) {
        if (r_error) { // if error requested, do not throw error
            return String();
        }
        ERR_FAIL_V_MSG(String(), "Can't open file from path '" + String(p_path) + "'.");
    }
    String data;
    data.resize(f->get_len());
    f->get_buffer((uint8_t *)data.data(), data.size());
    memdelete(f);
    return data;
}

String FileAccess::get_md5(StringView p_file) {

    FileAccess *f = FileAccess::open(p_file, READ);
    if (!f)
        return String();

    CryptoCore::MD5Context ctx;
    ctx.start();

    unsigned char step[32768];

    while (true) {

        int br = f->get_buffer(step, 32768);
        if (br > 0) {

            ctx.update(step, br);
        }
        if (br < 4096)
            break;
    }

    unsigned char hash[16];
    ctx.finish(hash);

    memdelete(f);

    return StringUtils::md5(hash);
}

String FileAccess::get_multiple_md5(const Vector<String> &p_file) {

    CryptoCore::MD5Context ctx;
    ctx.start();

    for (size_t i = 0; i < p_file.size(); i++) {
        FileAccess *f = FileAccess::open(p_file[i], READ);
        ERR_CONTINUE(!f);

        unsigned char step[32768];

        while (true) {

            int br = f->get_buffer(step, 32768);
            if (br > 0) {

                ctx.update(step, br);
            }
            if (br < 4096)
                break;
        }
        memdelete(f);
    }

    unsigned char hash[16];
    ctx.finish(hash);

    return StringUtils::md5(hash);
}

String FileAccess::get_sha256(StringView p_file) {

    FileAccess *f = FileAccess::open(p_file, READ);
    if (!f)
        return String();

    CryptoCore::SHA256Context ctx;
    ctx.start();

    unsigned char step[32768];

    while (true) {

        int br = f->get_buffer(step, 32768);
        if (br > 0) {

            ctx.update(step, br);
        }
        if (br < 4096)
            break;
    }

    unsigned char hash[32];
    ctx.finish(hash);

    memdelete(f);
    return StringUtils::hex_encode_buffer(hash, 32);
}

FileAccess::FileAccess() {

    endian_swap = false;
    real_is_double = false;
    _access_type = ACCESS_FILESYSTEM;
}
