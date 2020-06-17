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

#include "core/hash_map.h"
#include "core/string.h"
#include <QString>

enum Error : int;
namespace DocContents {
struct ArgumentDoc {

    String name;
    String type;
    String enumeration;
    String default_value;
};

struct MethodDoc {

    String name;
    String return_type;
    String return_enum;
    String qualifiers;
    String description;
    Vector<ArgumentDoc> arguments;
    bool operator<(const MethodDoc &p_md) const {
        return name.compare(p_md.name)<0;
    }
};

struct ConstantDoc {

    String name;
    String value;
    String enumeration;
    String description;
};

struct PropertyDoc {

    String name;
    String type;
    String enumeration;
    String description;
    String setter, getter;
    String default_value;
    bool overridden = false;
    bool operator<(const PropertyDoc &p_prop) const {
        return name < p_prop.name;
    }
};

//TODO: allow nested type/namespace docs.
struct ClassDoc {

    String name;
    String inherits;
    String category;
    String brief_description;
    String description;
    Vector<String> tutorials;
    Vector<MethodDoc> methods;
    Vector<MethodDoc> defined_signals;
    Vector<ConstantDoc> constants;
    Vector<PropertyDoc> properties;
    Vector<PropertyDoc> theme_properties;
    const ConstantDoc *const_by_name(const char * c_name) const {
        for(const ConstantDoc &cd : constants)
            if(cd.name==c_name)
                return &cd;
        return nullptr;
    }
    const ConstantDoc* const_by_enum_name(const char* c_name) const {
        Vector<StringView> parts;
        String::split_ref(parts,c_name,"::");
        if(parts.size()>2)
            return nullptr;
        if(parts.size()==1)
            return const_by_name(c_name);

        for (const ConstantDoc& cd : constants)
            if (cd.enumeration==parts[0] && cd.name == parts[1])
                return &cd;
        return nullptr;
    }
    const MethodDoc *func_by_name(StringView c_name) const {
        for (const MethodDoc& method_doc : methods)
            if (method_doc.name == c_name)
                return &method_doc;
        return nullptr;

    }

    const PropertyDoc * property_by_name(StringView c_name) const {
        for (const PropertyDoc& property_doc : properties)
            if (property_doc.name == c_name)
                return &property_doc;
        for (const PropertyDoc& property_doc : theme_properties)
            if (property_doc.name == c_name)
                return &property_doc;
        return nullptr;

    }
};

}
class DocData {
public:


    String version;
    String namespace_name;
    HashMap<String, DocContents::ClassDoc> class_list;
    HashMap<String, DocData *> child_namespaces;
    const DocContents::ClassDoc &class_doc(StringName sn) {
        return class_list[sn.asCString()];
    }
public:
    void merge_from(const DocData &p_data);
    void remove_from(const DocData &p_data);
    Error load_classes(QByteArray p_dir, bool recursively=false);
    static Error erase_classes(QByteArray p_dir, bool recursively=false);
    Error save_classes(QByteArray p_default_path, const char *version_branch, const HashMap<String, QString> &p_class_path);

    Error load_compressed(const uint8_t *p_data, int p_compressed_size, int p_uncompressed_size);
};

