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
#include "core/se_string.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"

FileAccess::CreateFunc FileAccess::create_func[ACCESS_MAX] = { nullptr, nullptr };

FileAccess::FileCloseFailNotify FileAccess::close_fail_notify = nullptr;

bool FileAccess::backup_save = false;

FileAccess *FileAccess::create(AccessType p_access) {

    ERR_FAIL_INDEX_V(p_access, ACCESS_MAX, nullptr)

    FileAccess *ret = create_func[p_access]();
    ret->_set_access_type(p_access);
    return ret;
}

bool FileAccess::exists(se_string_view p_name) {

    if (PackedData::get_singleton() && PackedData::get_singleton()->has_path(p_name))
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

const se_string &FileAccess::get_path() const { return null_se_string; }

const se_string &FileAccess::get_path_absolute() const { return null_se_string; };

FileAccess *FileAccess::create_for_path(se_string_view p_path) {

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

Error FileAccess::reopen(se_string_view p_path, int p_mode_flags) {

    return _open(p_path, p_mode_flags);
};

FileAccess *FileAccess::open(se_string_view p_path, int p_mode_flags, Error *r_error) {

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
};

se_string FileAccess::fix_path(se_string_view p_path) const {
    //helper used by file accesses that use a single filesystem

    se_string r_path(PathUtils::from_native_path(p_path));

    switch (_access_type) {

        case ACCESS_RESOURCES: {

            if (ProjectSettings::get_singleton()) {
                if (StringUtils::begins_with(r_path,"res://")) {

                    se_string resource_path = ProjectSettings::get_singleton()->get_resource_path();
                    if (!resource_path.empty()) {

                        return StringUtils::replace(r_path,"res:/", resource_path);
                    }
                    return StringUtils::replace(r_path,"res://", "");
                }
            }

        } break;
        case ACCESS_USERDATA: {

            if (StringUtils::begins_with(r_path,"user://")) {

                se_string data_dir = OS::get_singleton()->get_user_data_dir();
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
};

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
};

se_string FileAccess::get_token() const {

    se_string token;

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

class CharBuffer {
    Vector<char> vector;
    char stack_buffer[256];

    char *buffer = stack_buffer;
    int capacity;
    int written=0;

    bool grow() {

        if (vector.resize(next_power_of_2(1 + written)) != OK) {

            return false;
        }

        if (buffer == stack_buffer) { // first chunk?

            for (int i = 0; i < written; i++) {

                vector.write[i] = stack_buffer[i];
            }
        }

        buffer = vector.ptrw();
        capacity = vector.size();
        ERR_FAIL_COND_V(written >= capacity, false)

        return true;
    }

public:
    CharBuffer() :
            buffer(stack_buffer),
            capacity(sizeof(stack_buffer) / sizeof(char)) {
    }

    void push_back(char c) {

        if (written >= capacity) {

            ERR_FAIL_COND(!grow())
        }

        buffer[written++] = c;
    }

    const char *get_data() const {

        return buffer;
    }
};

se_string FileAccess::get_line() const {

    se_string line;

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

Vector<se_string> FileAccess::get_csv_line(char p_delim) const {

    se_string l;
    int qc = 0;
    do {
        if (eof_reached())
            break;

        l += get_line() + "\n";
        qc = 0;
        for (size_t i = 0; i < l.length(); i++) {

            if (l[i] == '"')
                qc++;
        }

    } while (qc % 2);

    l = StringUtils::substr(l,0, l.length() - 1);

    Vector<se_string> strings;

    bool in_quote = false;
    se_string current;
    for (size_t i = 0; i < l.length(); i++) {

        char c = l[i];
        if (!in_quote && c == p_delim) {
            strings.push_back(current);
            current.clear();
        } else if (c == '"') {
            if (l[i + 1] == '"') {
                current += '"';
                i++;
            } else {

                in_quote = !in_quote;
            }
        } else {
            current += c;
        }
    }

    strings.push_back(current);

    return strings;
}

int FileAccess::get_buffer(uint8_t *p_dst, int p_length) const {

    int i = 0;
    for (i = 0; i < p_length && !eof_reached(); i++)
        p_dst[i] = get_8();

    return i;
}

se_string FileAccess::get_as_utf8_string() const {
    se_string s;
    int len = get_len();
    s.resize(len + 1);

    int r = get_buffer((uint8_t *)s.data(), len);
    ERR_FAIL_COND_V(r != len, se_string())
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
};

void FileAccess::store_double(double p_dest) {

    MarshallDouble m;
    m.d = p_dest;
    store_64(m.l);
};

uint64_t FileAccess::get_modified_time(se_string_view p_file) {

    if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && PackedData::get_singleton()->has_path(p_file))
        return 0;

    FileAccess *fa = create_for_path(p_file);
    ERR_FAIL_COND_V_MSG(!fa, 0, "Cannot create FileAccess for path '" + se_string(p_file) + "'.")

    uint64_t mt = fa->_get_modified_time(p_file);
    memdelete(fa);
    return mt;
}

uint32_t FileAccess::get_unix_permissions(se_string_view p_file) {

    if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && PackedData::get_singleton()->has_path(p_file))
        return 0;

    FileAccess *fa = create_for_path(p_file);
    ERR_FAIL_COND_V_MSG(!fa, 0, "Cannot create FileAccess for path '" + se_string(p_file) + "'.")

    uint32_t mt = fa->_get_unix_permissions(p_file);
    memdelete(fa);
    return mt;
}

Error FileAccess::set_unix_permissions(se_string_view p_file, uint32_t p_permissions) {

    FileAccess *fa = create_for_path(p_file);
    ERR_FAIL_COND_V_MSG(!fa, ERR_CANT_CREATE, "Cannot create FileAccess for path '" + se_string(p_file) + "'.")

    Error err = fa->_set_unix_permissions(p_file, p_permissions);
    memdelete(fa);
    return err;
}

void FileAccess::store_string(se_string_view p_string) {

    if (p_string.empty())
        return;

    store_buffer((const uint8_t *)p_string.data(), p_string.length());
}

void FileAccess::store_pascal_string(se_string_view p_string) {

    store_32(p_string.length());
    store_buffer((const uint8_t *)p_string.data(), p_string.length());
};

se_string FileAccess::get_pascal_string() {

    uint32_t sl = get_32();
    se_string cs;
    cs.resize(sl);
    get_buffer((uint8_t *)cs.data(), sl);
    return cs;
};

void FileAccess::store_line(se_string_view p_line) {

    store_string(p_line);
    store_8('\n');
}

void FileAccess::store_csv_line(const PODVector<se_string> &p_values, char p_delim) {

    se_string line;
    int size = p_values.size();
    for (int i = 0; i < size; ++i) {
        se_string value = p_values[i];

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

void FileAccess::store_buffer(const uint8_t *p_src, int p_length) {

    for (int i = 0; i < p_length; i++)
        store_8(p_src[i]);
}

PODVector<uint8_t> FileAccess::get_file_as_array(se_string_view p_path, Error *r_error) {

    FileAccess *f = FileAccess::open(p_path, READ, r_error);
    if (!f) {
        if (r_error) { // if error requested, do not throw error
            return PODVector<uint8_t>();
        }
        ERR_FAIL_V_MSG(PODVector<uint8_t>(), "Can't open file from path '" + se_string(p_path) + "'.")
    }
    PODVector<uint8_t> data;
    data.resize(f->get_len());
    f->get_buffer(data.data(), data.size());
    memdelete(f);
    return data;
}

se_string FileAccess::get_file_as_string(se_string_view p_path, Error *r_error) {

    FileAccess *f = FileAccess::open(p_path, READ, r_error);
    if (!f) {
        if (r_error) { // if error requested, do not throw error
            return se_string();
        }
        ERR_FAIL_V_MSG(se_string(), "Can't open file from path '" + se_string(p_path) + "'.")
    }
    se_string data;
    data.resize(f->get_len());
    f->get_buffer((uint8_t *)data.data(), data.size());
    memdelete(f);
    return data;
}

se_string FileAccess::get_md5(se_string_view p_file) {

    FileAccess *f = FileAccess::open(p_file, READ);
    if (!f)
        return se_string();

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

se_string FileAccess::get_multiple_md5(const PODVector<se_string> &p_file) {

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

se_string FileAccess::get_sha256(se_string_view p_file) {

    FileAccess *f = FileAccess::open(p_file, READ);
    if (!f)
        return se_string();

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
};
