/*************************************************************************/
/*  config_file.h                                                        */
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

#include "core/ordered_hash_map.h"
#include "core/map.h"
#include "core/reference.h"

class FileAccess;

class GODOT_EXPORT ConfigFile : public RefCounted {

    GDCLASS(ConfigFile, RefCounted)

    Map<se_string, Map<se_string, Variant> > values;

    PoolSeStringArray _get_sections() const;
    PoolSeStringArray _get_section_keys(se_string_view p_section) const;
    Error _internal_load(se_string_view p_path, FileAccess *f);
    Error _internal_save(FileAccess *file);

protected:
    static void _bind_methods();

public:
    const Map<se_string, Map<se_string, Variant> > &all_values() const { return values; }
    void set_value(se_string_view p_section, se_string_view p_key, const Variant &p_value);
    Variant get_value(se_string_view p_section, se_string_view p_key, const Variant& p_default = Variant()) const;

    bool has_section(se_string_view p_section) const;
    bool has_section_key(se_string_view p_section, se_string_view p_key) const;

    void get_sections(PODVector<se_string> *r_sections) const;
    void get_section_keys(se_string_view p_section, PODVector<se_string> *r_keys) const;

    void erase_section(se_string_view p_section);
    void erase_section_key(se_string_view p_section, se_string_view p_key);

    Error save(se_string_view p_path);
    Error load(se_string_view p_path);

    Error load_encrypted(se_string_view p_path, const Vector<uint8_t> &p_key);
    Error load_encrypted_pass(se_string_view p_path, se_string_view p_pass);

    Error save_encrypted(se_string_view p_path, const Vector<uint8_t> &p_key);
    Error save_encrypted_pass(se_string_view p_path, se_string_view p_pass);

    ConfigFile();
};
