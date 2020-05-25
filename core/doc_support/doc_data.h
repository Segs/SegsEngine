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
#include <QHash>

enum Error : int;
namespace DocContents {
struct ArgumentDoc {

    QString name;
    QString type;
    QString enumeration;
    QString default_value;
};

struct MethodDoc {

    QString name;
    QString return_type;
    QString return_enum;
    QString qualifiers;
    QString description;
    Vector<ArgumentDoc> arguments;
    bool operator<(const MethodDoc &p_md) const {
        return name < p_md.name;
    }
};

struct ConstantDoc {

    QString name;
    QString value;
    QString enumeration;
    QString description;
};

struct PropertyDoc {

    QString name;
    QString type;
    QString enumeration;
    QString description;
    QString setter, getter;
    QString default_value;
    bool overridden = false;
    bool operator<(const PropertyDoc &p_prop) const {
        return name < p_prop.name;
    }
};
struct ClassDoc {

    QString name;
    QString inherits;
    QString category;
    QString brief_description;
    QString description;
    Vector<QString> tutorials;
    Vector<MethodDoc> methods;
    Vector<MethodDoc> defined_signals;
    Vector<ConstantDoc> constants;
    Vector<PropertyDoc> properties;
    Vector<PropertyDoc> theme_properties;
    const ConstantDoc *by_name(const char * name) {
        for(const ConstantDoc &cd : constants)
            if(cd.name==name)
                return &cd;
        return nullptr;
    }
};

}
class DocData {
public:


    String version;

    QHash<QString, DocContents::ClassDoc> class_list;
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

