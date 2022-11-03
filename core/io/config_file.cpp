/*************************************************************************/
/*  config_file.cpp                                                      */
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

#include "config_file.h"

#include "core/io/file_access_encrypted.h"
#include "core/os/keyboard.h"
#include "core/variant_parser.h"
#include "core/method_bind.h"
#include "core/string_utils.inl"
#include "core/pool_vector.h"
#include "core/string_formatter.h"

#include "EASTL/unique_ptr.h"

IMPL_GDCLASS(ConfigFile)

PoolStringArray ConfigFile::_get_section_keys(StringView p_section) const {

    Vector<String> s = get_section_keys(p_section);
    PoolStringArray arr;
    arr.resize(s.size());
    int idx = 0;
    for (const String &E : s) {

        arr.set(idx++, E);
    }

    return arr;
}

void ConfigFile::set_value(StringView _section, StringView _key, const Variant &p_value) {
    String p_section(_section);
    String p_key(_key);
    if (p_value.get_type() == VariantType::NIL) {
        //erase
        if (!values.contains(p_section))
            return; // ?
        values[p_section].erase(p_key);
        if (values[p_section].empty()) {
            values.erase(p_section);
        }

    } else {
        if (!values.contains(p_section)) {
            values[p_section] = {};
        }

        values[p_section][p_key] = p_value;
    }
}
Variant ConfigFile::get_value(StringView _section, StringView _key, const Variant &p_default) const {
    String p_section(_section);
    String p_key(_key);
    auto iter_section = values.find(p_section);
    if (iter_section == values.end()) {
        ERR_FAIL_COND_V_MSG(p_default.get_type() == VariantType::NIL, Variant(),
                FormatSN("Couldn't find the given section \"%.*s\" and key \"%.*s\", and no default was given.",
                        (int)p_section.size(), p_section.data(), (int)p_key.size(), p_key.data()));
        return p_default;
    }
    auto iter_key = iter_section->second.find(p_key);
    if (iter_key == iter_section->second.end()) {
        ERR_FAIL_COND_V_MSG(p_default.get_type() == VariantType::NIL, p_default,
                "Couldn't find the given section/key and no default was given.");
        return p_default;
    }
    return iter_key->second;
}

bool ConfigFile::has_section(StringView _section) const {

    return values.contains_as(_section);
}
bool ConfigFile::has_section_key(StringView p_section, StringView p_key) const {
    auto iter=values.find_as(p_section);
    if (iter==values.end())
        return false;
    return iter->second.contains_as(p_key);
}

Vector<String> ConfigFile::get_sections() const {
    return values.keys();
}
Vector<String> ConfigFile::get_section_keys(StringView _section) const {
    const String p_section(_section);
    auto iter = values.find_as(_section);

    ERR_FAIL_COND_V_MSG(iter==values.end(), {},"Cannont get keys from nonexistent section '" + p_section + "'.");
    return iter->second.keys();
}
void ConfigFile::erase_section(StringView p_section) {

    ERR_FAIL_COND_MSG(!values.contains_as(p_section),
            FormatSN("Cannot erase nonexistent section \"%.*s\".", (int)p_section.size(), p_section.data()));
    values.erase(String(p_section));
}
void ConfigFile::erase_section_key(StringView p_section, StringView p_key) {

    auto iter=values.find_as(p_section);
    ERR_FAIL_COND_MSG(iter == values.end(), FormatSN("Cannot erase key '%.*s' from nonexistent section '%.*s'.",
                                                    (int)p_key.size(), p_key.data(), (int)p_section.size(), p_section.data()));

    ERR_FAIL_COND_MSG(!iter->second.contains_as(p_key), FormatSN("Cannot erase nonexistent key \"%.*s\" from section \"%.*s\".",
                                                                 (int)p_key.size(), p_key.data(), (int)p_section.size(), p_section.data()));

    iter->second.erase_as(p_key);
}

Error ConfigFile::save(StringView p_path) {

    Error err;
    FileAccess *file = FileAccess::open(p_path, FileAccess::WRITE, &err);

    if (err) {
        memdelete(file);
        return err;
    }

    return _internal_save(file);
}

Error ConfigFile::save_encrypted(StringView p_path, const Vector<uint8_t> &p_key) {

    Error err;
    FileAccess *f = FileAccess::open(p_path, FileAccess::WRITE, &err);

    if (err)
        return err;

    FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
    err = fae->open_and_parse(f, p_key, FileAccessEncrypted::MODE_WRITE_AES256);
    if (err) {
        memdelete(fae);
        memdelete(f);
        return err;
    }
    return _internal_save(fae);
}

Error ConfigFile::save_encrypted_pass(StringView p_path, StringView p_pass) {

    Error err;
    FileAccess *f = FileAccess::open(p_path, FileAccess::WRITE, &err);

    if (err)
        return err;

    FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
    err = fae->open_and_parse_password(f, p_pass, FileAccessEncrypted::MODE_WRITE_AES256);
    if (err) {
        memdelete(fae);
        memdelete(f);
        return err;
    }

    return _internal_save(fae);
}

Error ConfigFile::_internal_save(FileAccess *file) {

    bool first=true;
    for (const eastl::pair<const String, Map<String, Variant> > &E : values) {

        if (!first)
            file->store_string("\n");
        first = false;
        file->store_string("[" + E.first + "]\n\n");

        for (const eastl::pair<const String, Variant> &F : E.second) {

            String vstr;
            VariantWriter::write_to_string(F.second, vstr);
            file->store_string(F.first + "=" + vstr + "\n");
        }
    }

    memdelete(file);

    return OK;
}

Error ConfigFile::load(StringView p_path) {

    Error err;
    FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);

    if (!f)
        return err;

    return _internal_load(p_path, f);
}

Error ConfigFile::load_encrypted(StringView p_path, const Vector<uint8_t> &p_key) {

    Error err;
    FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);

    if (err)
        return err;

    FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
    err = fae->open_and_parse(f, p_key, FileAccessEncrypted::MODE_READ);
    if (err) {
        memdelete(fae);
        memdelete(f);
        return err;
    }
    return _internal_load(p_path, fae);
}

Error ConfigFile::load_encrypted_pass(StringView p_path, StringView p_pass) {

    Error err;
    FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);

    if (err)
        return err;

    FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
    err = fae->open_and_parse_password(f, p_pass, FileAccessEncrypted::MODE_READ);
    if (err) {
        memdelete(fae);
        memdelete(f);
        return err;
    }

    return _internal_load(p_path, fae);
}

Error ConfigFile::_internal_load(StringView p_path, FileAccess *f) {

    eastl::unique_ptr<VariantParserStream,wrap_deleter> vps(VariantParser::get_file_stream(f));
    Error err = _parse(p_path, vps.get());
    vps.reset();
    memdelete(f);

    return err;
}

Error ConfigFile::parse(const String &p_data) {

    eastl::unique_ptr<VariantParserStream,wrap_deleter> vps(VariantParser::get_string_stream(eastl::move(String(p_data))));
    return _parse("<string>", vps.get());
}

Error ConfigFile::_parse(StringView p_path, VariantParserStream *p_stream) {

    String assign;
    Variant value;
    VariantParser::Tag next_tag;

    int lines = 0;
    String error_text;

    String section;

    while (true) {

        assign = Variant().as<String>();
        next_tag.fields.clear();
        next_tag.name.clear();

        Error err = VariantParser::parse_tag_assign_eof(
                p_stream, lines, error_text, next_tag, assign, value, nullptr, true);
        if (err == ERR_FILE_EOF) {
            return OK;
        } else if (err != OK) {
            ERR_PRINT(FormatSN(
                    "ConfigFile parse error at %.*s:%d: %s.", (int)p_path.size(), p_path.data(), lines, error_text.c_str()));
            return err;
        }

        if (!assign.empty()) {
            set_value(section, assign, value);
        } else if (!next_tag.name.empty()) {
            section = next_tag.name;
        }
    }
    return OK;
}

void ConfigFile::clear() {
    values.clear();
}

void ConfigFile::_bind_methods() {

    SE_BIND_METHOD(ConfigFile,set_value);
    MethodBinder::bind_method(D_METHOD("get_value", {"section", "key", "default"}), &ConfigFile::get_value, {DEFVAL(Variant())});

    SE_BIND_METHOD(ConfigFile,has_section);
    SE_BIND_METHOD(ConfigFile,has_section_key);

    SE_BIND_METHOD(ConfigFile,get_sections);
    MethodBinder::bind_method(D_METHOD("get_section_keys", {"section"}), &ConfigFile::_get_section_keys);

    SE_BIND_METHOD(ConfigFile,erase_section);
    SE_BIND_METHOD(ConfigFile,erase_section_key);

    SE_BIND_METHOD(ConfigFile,load);
    SE_BIND_METHOD(ConfigFile,parse);
    SE_BIND_METHOD(ConfigFile,save);

    SE_BIND_METHOD(ConfigFile,load_encrypted);
    SE_BIND_METHOD(ConfigFile,load_encrypted_pass);

    SE_BIND_METHOD(ConfigFile,save_encrypted);
    SE_BIND_METHOD(ConfigFile,save_encrypted_pass);
    SE_BIND_METHOD(ConfigFile,clear);
}
