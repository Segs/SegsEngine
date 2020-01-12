/*************************************************************************/
/*  doc_data.h                                                           */
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

#pragma once

#include "core/io/xml_parser.h"
#include "core/map.h"
#include "core/se_string.h"

class DocData {
public:
    struct ArgumentDoc {

        se_string name;
        se_string type;
        se_string enumeration;
        se_string default_value;
    };

    struct MethodDoc {

        se_string name;
        se_string return_type;
        se_string return_enum;
        se_string qualifiers;
        se_string description;
        Vector<ArgumentDoc> arguments;
        bool operator<(const MethodDoc &p_md) const {
            return name < p_md.name;
        }
    };

    struct ConstantDoc {

        se_string name;
        se_string value;
        se_string enumeration;
        se_string description;
    };

    struct PropertyDoc {

        se_string name;
        StringName type;
        se_string enumeration;
        se_string description;
        se_string setter, getter;
        se_string default_value;
        bool overridden = false;
        bool operator<(const PropertyDoc &p_prop) const {
            return name < p_prop.name;
        }
    };

    struct ClassDoc {

        StringName name;
        StringName inherits;
        se_string category;
        se_string brief_description;
        se_string description;
        Vector<se_string> tutorials;
        Vector<MethodDoc> methods;
        Vector<MethodDoc> defined_signals;
        Vector<ConstantDoc> constants;
        Vector<PropertyDoc> properties;
        Vector<PropertyDoc> theme_properties;
    };

    se_string version;

    Map<StringName, ClassDoc> class_list;
    Error _load(Ref<XMLParser> parser);

public:
    void merge_from(const DocData &p_data);
    void remove_from(const DocData &p_data);
    void generate(bool p_basic_types = false);
    Error load_classes(se_string_view p_dir);
    static Error erase_classes(se_string_view p_dir);
    Error save_classes(se_string_view p_default_path, const Map<StringName, se_string> &p_class_path);

    Error load_compressed(const uint8_t *p_data, int p_compressed_size, int p_uncompressed_size);
};

