/*************************************************************************/
/*  config_file.cpp                                                      */
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

#include "config_file.h"

#include "core/io/file_access_encrypted.h"
#include "core/os/keyboard.h"
#include "core/variant_parser.h"
#include "core/method_bind.h"
#include "core/string_utils.inl"


IMPL_GDCLASS(ConfigFile)

PoolSeStringArray ConfigFile::_get_sections() const {

    List<se_string> s;
    get_sections(&s);
    PoolSeStringArray arr;
    arr.resize(s.size());
    int idx = 0;
    for (const List<se_string>::Element *E = s.front(); E; E = E->next()) {

        arr.set(idx++, Variant(E->deref()));
    }

    return arr;
}

PoolSeStringArray ConfigFile::_get_section_keys(se_string_view p_section) const {

    List<se_string> s;
    get_section_keys(p_section, &s);
    PoolSeStringArray arr;
    arr.resize(s.size());
    int idx = 0;
    for (const List<se_string>::Element *E = s.front(); E; E = E->next()) {

        arr.set(idx++, Variant(E->deref()));
    }

    return arr;
}

void ConfigFile::set_value(se_string_view _section, se_string_view _key, const Variant &p_value) {
    se_string p_section(_section);
    se_string p_key(_key);
    if (p_value.get_type() == VariantType::NIL) {
        //erase
        if (!values.has(p_section))
            return; // ?
        values[p_section].erase(p_key);
        if (values[p_section].empty()) {
            values.erase(p_section);
        }

    } else {
        if (!values.has(p_section)) {
            values[p_section] = OrderedHashMap<se_string, Variant>();
        }

        values[p_section][p_key] = p_value;
    }
}
Variant ConfigFile::get_value(se_string_view _section, se_string_view _key, const Variant& p_default) const {
    se_string p_section(_section);
    se_string p_key(_key);

    if (!values.has(p_section) || !values[p_section].has(p_key)) {
        ERR_FAIL_COND_V_MSG(p_default.get_type() == VariantType::NIL, p_default, "Couldn't find the given section/key and no default was given.")
        return p_default;
    }
    return values[p_section][p_key];
}

bool ConfigFile::has_section(se_string_view _section) const {

    return values.has(se_string(_section));
}
bool ConfigFile::has_section_key(se_string_view p_section, se_string_view p_key) const {

    if (!values.has(se_string(p_section)))
        return false;
    return values[se_string(p_section)].has(se_string(p_key));
}

void ConfigFile::get_sections(List<se_string> *r_sections) const {

    for (OrderedHashMap<se_string, OrderedHashMap<se_string, Variant> >::ConstElement E = values.front(); E; E = E.next()) {
        r_sections->push_back(E.key());
    }
}
void ConfigFile::get_section_keys(se_string_view _section, List<se_string> *r_keys) const {
    const se_string p_section(_section);
    ERR_FAIL_COND_MSG(!values.has(p_section), "Cannont get keys from nonexistent section '" + p_section + "'.")

    for (OrderedHashMap<se_string, Variant>::ConstElement E = values[p_section].front(); E; E = E.next()) {
        r_keys->push_back(E.key());
    }
}
void ConfigFile::get_section_keys_utf8(se_string_view _section, PODVector<se_string> &r_keys) const {
    const se_string p_section(_section);
    ERR_FAIL_COND_MSG(!values.has(p_section), "Cannont get keys from nonexistent section '" + p_section + "'.")
    r_keys.reserve(r_keys.size()+values.size());
    for (OrderedHashMap<se_string, Variant>::ConstElement E = values[p_section].front(); E; E = E.next()) {
        r_keys.push_back(E.key());
    }
}
void ConfigFile::erase_section(se_string_view p_section) {

    values.erase(se_string(p_section));
}
void ConfigFile::erase_section_key(se_string_view p_section, se_string_view p_key) {

    ERR_FAIL_COND_MSG(!values.has(se_string(p_section)), "Cannot erase key from nonexistent section '" + se_string(p_section) + "'.")

    values[se_string(p_section)].erase(se_string(p_key));
}

Error ConfigFile::save(se_string_view p_path) {

    Error err;
    FileAccess *file = FileAccess::open(p_path, FileAccess::WRITE, &err);

    if (err) {
        if (file)
            memdelete(file);
        return err;
    }

    return _internal_save(file);
}

Error ConfigFile::save_encrypted(se_string_view p_path, const Vector<uint8_t> &p_key) {

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

Error ConfigFile::save_encrypted_pass(se_string_view p_path, se_string_view p_pass) {

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

    for (OrderedHashMap<se_string, OrderedHashMap<se_string, Variant> >::Element E = values.front(); E; E = E.next()) {

        if (E != values.front())
            file->store_string("\n");
        file->store_string("[" + E.key() + "]\n\n");

        for (OrderedHashMap<se_string, Variant>::Element F = E.get().front(); F; F = F.next()) {

            se_string vstr;
            VariantWriter::write_to_string(F.get(), vstr);
            file->store_string(F.key() + "=" + vstr + "\n");
        }
    }

    memdelete(file);

    return OK;
}

Error ConfigFile::load(se_string_view p_path) {

    Error err;
    FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);

    if (!f)
        return err;

    return _internal_load(p_path, f);
}

Error ConfigFile::load_encrypted(se_string_view p_path, const Vector<uint8_t> &p_key) {

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

Error ConfigFile::load_encrypted_pass(se_string_view p_path, se_string_view p_pass) {

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

Error ConfigFile::_internal_load(se_string_view p_path, FileAccess *f) {

    VariantParser::Stream *stream=VariantParser::get_file_stream(f);

    se_string assign;
    Variant value;
    VariantParser::Tag next_tag;

    int lines = 0;
    se_string error_text;

    se_string_view section;

    while (true) {

        assign = Variant().as<se_string>();
        next_tag.fields.clear();
        next_tag.name.clear();

        Error err = VariantParser::parse_tag_assign_eof(stream, lines, error_text, next_tag, assign, value, nullptr, true);
        if (err == ERR_FILE_EOF) {
            VariantParser::release_stream(stream);
            memdelete(f);
            return OK;
        } else if (err != OK) {
            ERR_PRINT("ConfgFile::load - " + se_string(p_path) + ":" + ::to_string(lines) +
                      " error: " + error_text + ".")
            VariantParser::release_stream(stream);
            memdelete(f);
            return err;
        }

        if (!assign.empty()) {
            set_value(section, assign, value);
        } else if (!next_tag.name.empty()) {
            section = next_tag.name;
        }
    }
}

void ConfigFile::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_value", {"section", "key", "value"}), &ConfigFile::set_value);
    MethodBinder::bind_method(D_METHOD("get_value", {"section", "key", "default"}), &ConfigFile::get_value, {DEFVAL(Variant())});

    MethodBinder::bind_method(D_METHOD("has_section", {"section"}), &ConfigFile::has_section);
    MethodBinder::bind_method(D_METHOD("has_section_key", {"section", "key"}), &ConfigFile::has_section_key);

    MethodBinder::bind_method(D_METHOD("get_sections"), &ConfigFile::_get_sections);
    MethodBinder::bind_method(D_METHOD("get_section_keys", {"section"}), &ConfigFile::_get_section_keys);

    MethodBinder::bind_method(D_METHOD("erase_section", {"section"}), &ConfigFile::erase_section);
    MethodBinder::bind_method(D_METHOD("erase_section_key", {"section", "key"}), &ConfigFile::erase_section_key);

    MethodBinder::bind_method(D_METHOD("load", {"path"}), &ConfigFile::load);
    MethodBinder::bind_method(D_METHOD("save", {"path"}), &ConfigFile::save);

    MethodBinder::bind_method(D_METHOD("load_encrypted", {"path", "key"}), &ConfigFile::load_encrypted);
    MethodBinder::bind_method(D_METHOD("load_encrypted_pass", {"path", "pass"}), &ConfigFile::load_encrypted_pass);

    MethodBinder::bind_method(D_METHOD("save_encrypted", {"path", "key"}), &ConfigFile::save_encrypted);
    MethodBinder::bind_method(D_METHOD("save_encrypted_pass", {"path", "pass"}), &ConfigFile::save_encrypted_pass);
}

ConfigFile::ConfigFile() = default;
