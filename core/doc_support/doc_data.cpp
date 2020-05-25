/*************************************************************************/
/*  doc_data.cpp                                                         */
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

#include "doc_data.h"

#include "EASTL/sort.h"
#include <QString>
#include <QtCore/QXmlStreamReader>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>


#define ERR_FILE_CORRUPT Error(16)
#define ERR_FILE_CANT_OPEN Error(12)
#define OK Error(0);

using namespace DocContents;

Error _load(QXmlStreamReader &parser,DocData &tgt);

void DocData::merge_from(const DocData &p_data) {

    for (ClassDoc &c : class_list) {

        auto iter = p_data.class_list.find(c.name);
        if (iter==p_data.class_list.end())
            continue;

        const ClassDoc &cf = *iter;

        c.description = cf.description;
        c.brief_description = cf.brief_description;
        c.tutorials = cf.tutorials;

        for (MethodDoc &m : c.methods) {

            for (int j = 0; j < cf.methods.size(); j++) {

                if (cf.methods[j].name != m.name)
                    continue;
                if (cf.methods[j].arguments.size() != m.arguments.size())
                    continue;
                // since polymorphic functions are allowed we need to check the type of
                // the arguments so we make sure they are different.
                int arg_count = cf.methods[j].arguments.size();
                Vector<bool> arg_used;
                arg_used.resize(arg_count);
                for (int l = 0; l < arg_count; ++l)
                    arg_used[l] = false;
                // also there is no guarantee that argument ordering will match, so we
                // have to check one by one so we make sure we have an exact match
                for (int k = 0; k < arg_count; ++k) {
                    for (int l = 0; l < arg_count; ++l)
                        if (cf.methods[j].arguments[k].type == m.arguments[l].type && !arg_used[l]) {
                            arg_used[l] = true;
                            break;
                        }
                }
                bool not_the_same = false;
                for (int l = 0; l < arg_count; ++l)
                    if (!arg_used[l]) // at least one of the arguments was different
                        not_the_same = true;
                if (not_the_same)
                    continue;

                const MethodDoc &mf = cf.methods[j];

                m.description = mf.description;
                break;
            }
        }

        for (int i = 0; i < c.defined_signals.size(); i++) {

            MethodDoc &m = c.defined_signals[i];

            for (int j = 0; j < cf.defined_signals.size(); j++) {

                if (cf.defined_signals[j].name != m.name)
                    continue;
                const MethodDoc &mf = cf.defined_signals[j];

                m.description = mf.description;
                break;
            }
        }

        for (ConstantDoc &m : c.constants) {

            for (int j = 0; j < cf.constants.size(); j++) {

                if (cf.constants[j].name != m.name)
                    continue;
                const ConstantDoc &mf = cf.constants[j];

                m.description = mf.description;
                break;
            }
        }

        for (PropertyDoc &p : c.properties) {

            for (int j = 0; j < cf.properties.size(); j++) {

                if (cf.properties[j].name != p.name)
                    continue;
                const PropertyDoc &pf = cf.properties[j];

                p.description = pf.description;
                break;
            }
        }

        for (PropertyDoc &p : c.theme_properties) {

            for (int j = 0; j < cf.theme_properties.size(); j++) {

                if (cf.theme_properties[j].name != p.name)
                    continue;
                const PropertyDoc &pf = cf.theme_properties[j];

                p.description = pf.description;
                break;
            }
        }
    }
}

void DocData::remove_from(const DocData &p_data) {
    for (const QString &E : p_data.class_list.keys()) {
        if (class_list.contains(E))
            class_list.remove(E);
    }
}

static Error _parse_methods(QXmlStreamReader &parser, Vector<DocContents::MethodDoc> &methods) {

    auto section = parser.name();
    const auto element(parser.name().mid(0,parser.name().size()-1));

    while (!parser.atEnd()) {

        if (parser.tokenType() == QXmlStreamReader::StartElement) {

            if (parser.name() == element) {

                DocContents::MethodDoc method;
                const auto &attrs(parser.attributes());
                if(!attrs.hasAttribute("name")) {
                    qCritical("missing 'name' attribute");
                    return ERR_FILE_CORRUPT;
                }
                method.name = attrs.value("name").toString();
                if (attrs.hasAttribute("qualifiers"))
                    method.qualifiers = attrs.value("qualifiers").toString();

                while (!parser.atEnd()) {

                    if (parser.tokenType() == QXmlStreamReader::StartElement) {

                        const auto & name(parser.name());
                        const auto &attrs(parser.attributes());
                        if (name == "return") {

                            if(!attrs.hasAttribute("type")) {
                                qCritical("missing 'type' attribute");
                                return ERR_FILE_CORRUPT;
                            }

                            method.return_type = attrs.value("type").toString();
                            if (attrs.hasAttribute("enum")) {
                                method.return_enum = attrs.value("enum").toString();
                            }
                        } else if (name == "argument") {

                            DocContents::ArgumentDoc argument;
                            if(!attrs.hasAttribute("name") || !attrs.hasAttribute("type")) {
                                qCritical("missing 'name' or 'type' attribute");
                                return ERR_FILE_CORRUPT;
                            }

                            argument.name = attrs.value("name").toString();
                            argument.type = attrs.value("type").toString();
                            if (attrs.hasAttribute("enum")) {
                                argument.enumeration = attrs.value("enum").toString();
                            }

                            method.arguments.push_back(argument);

                        } else if (name == "description") {
                            if (parser.readNext() == QXmlStreamReader::Characters)
                                method.description = parser.text().toString();
                        }

                    } else if (parser.tokenType() == QXmlStreamReader::EndElement && parser.name() == element)
                        break;
                }

                methods.push_back(method);

            } else {
                qCritical().noquote()<<"Invalid tag in doc file: " << parser.name() + ".";
                return ERR_FILE_CORRUPT;
            }

        } else if (parser.tokenType() == QXmlStreamReader::EndElement && parser.name() == section)
            break;
    }

    return OK;
}

Error DocData::load_classes(QByteArray p_dir, bool recursively) {

    if(!QFile::exists(p_dir) || !QFileInfo(p_dir).isDir()) {
        return ERR_FILE_CANT_OPEN;
    }

    QDirIterator fl(p_dir,{"*.xml"},QDir::NoFilter,recursively ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);

    while(fl.hasNext()) {
        QString name=fl.next();
        auto fi(fl.fileInfo());
        if(fi.isFile()) {
            QFile src_file(name);
            if(!src_file.open(QFile::ReadOnly)) {
                qCritical() << "Failed to load doc source file"<<name;
            }
            QXmlStreamReader reader(&src_file);
            _load(reader,*this);
        }
    }
    return OK;
}
Error DocData::erase_classes(QByteArray p_dir, bool recursively) {

    if(!QFile::exists(p_dir) || !QFileInfo(p_dir).isDir()) {
        return ERR_FILE_CANT_OPEN;
    }

    QDirIterator fl(p_dir,{"*.xml"},QDir::NoFilter,recursively ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);

    while(fl.hasNext()) {
        QString name=fl.next();
        auto fi(fl.fileInfo());
        if(fi.isFile()) {
            QFile::remove(name);
        }
    }
    return OK;
}
Error _load(QXmlStreamReader &parser,DocData &tgt) {

    while (!parser.atEnd()) {

        QXmlStreamReader::TokenType tt = parser.readNext();
        if (tt == QXmlStreamReader::StartElement && parser.name() == "?xml") {
            parser.skipCurrentElement();
        }

        if (parser.tokenType() != QXmlStreamReader::StartElement)
            continue; //no idea what this may be, but skipping anyway

        const auto class_attrs(parser.attributes());
        if(parser.name() != "class" || !class_attrs.hasAttribute("name")) {
            qCritical("Non-class first xml element or missing 'name' attribute");
            return ERR_FILE_CORRUPT;
        }
        QString name(class_attrs.value("name").toString());
        tgt.class_list[name] = DocContents::ClassDoc();
        DocContents::ClassDoc &c = tgt.class_list[name];

        c.name = name;
        if (class_attrs.hasAttribute("inherits"))
            c.inherits = class_attrs.value("inherits").toString();

        while (parser.readNext() != QXmlStreamReader::Invalid) {

            if (parser.tokenType() == QXmlStreamReader::StartElement) {

                const auto name2 = parser.name();

                if (name2 == "brief_description") {

                    parser.readNext();
                    if (parser.tokenType() == QXmlStreamReader::Characters)
                        c.brief_description = parser.text().toString();

                } else if (name2 == "description") {
                    parser.readNext();
                    if (parser.tokenType() == QXmlStreamReader::Characters)
                        c.description = parser.text().toString();
                } else if (name2 == "tutorials") {
                    while (parser.readNext() != QXmlStreamReader::Invalid) {

                        if (parser.tokenType() == QXmlStreamReader::StartElement) {

                            const QStringRef name3 = parser.name();

                            if (name3 == "link") {

                                parser.readNext();
                                if (parser.tokenType() == QXmlStreamReader::Characters)
                                    c.tutorials.emplace_back(parser.text().trimmed().toString());
                            } else {

                                qCritical().noquote()<<"Invalid tag in doc file: " + name3 + ".";
                                return ERR_FILE_CORRUPT;
                            }
                        } else if (parser.tokenType() == QXmlStreamReader::EndElement && parser.name() == "tutorials")
                            break; // End of <tutorials>.
                    }
                } else if (name2 == "methods") {

                    Error err2 = _parse_methods(parser, c.methods);
                    ERR_FAIL_COND_V(err2, err2);

                } else if (name2 == "signals") {

                    Error err2 = _parse_methods(parser, c.defined_signals);
                    ERR_FAIL_COND_V(err2, err2);
                } else if (name2 == "members") {
                    bool in_item=false;
                    while (parser.readNext() != QXmlStreamReader::Invalid) {

                        if (parser.tokenType() == QXmlStreamReader::StartElement) {

                            const QStringRef name3 = parser.name();

                            if (name3 == "member") {
                                in_item = true;
                                DocContents::PropertyDoc prop2;
                                const auto attrs(parser.attributes());
                                ERR_FAIL_COND_V(!attrs.hasAttribute("name"), ERR_FILE_CORRUPT);
                                prop2.name = attrs.value("name").toString();
                                ERR_FAIL_COND_V(!attrs.hasAttribute("type"), ERR_FILE_CORRUPT);
                                prop2.type = attrs.value("type").toString();
                                if (attrs.hasAttribute("setter"))
                                    prop2.setter = attrs.value("setter").toString();
                                if (attrs.hasAttribute("getter"))
                                    prop2.getter = attrs.value("getter").toString();
                                if (attrs.hasAttribute("enum"))
                                    prop2.enumeration = attrs.value("enum").toString();
                                c.properties.push_back(prop2);
                            } else {
                                qCritical()<<"Invalid tag in doc file: " << name3 << ".";
                                return ERR_FILE_CORRUPT;
                            }

                        } else if (parser.tokenType() == QXmlStreamReader::Characters) {
                            if(in_item)
                                c.properties.back().description = parser.text().toString();

                        } else if (parser.tokenType() == QXmlStreamReader::EndElement) {
                            in_item  = false;
                            if(parser.name() == "members")
                                break; // End of <members>.
                        }
                    }

                } else if (name2 == "theme_items") {
                    bool in_theme_item=false;
                    while (parser.readNext() != QXmlStreamReader::Invalid) {

                        if (parser.tokenType() == QXmlStreamReader::StartElement) {
                            const QStringRef name3 = parser.name();

                            if (name3 == "theme_item") {
                                in_theme_item = true;

                                DocContents::PropertyDoc prop2;
                                const auto attrs(parser.attributes());
                                ERR_FAIL_COND_V(!attrs.hasAttribute("name"), ERR_FILE_CORRUPT);
                                prop2.name = attrs.value("name").toString();
                                ERR_FAIL_COND_V(!attrs.hasAttribute("type"), ERR_FILE_CORRUPT);
                                prop2.type = attrs.value("type").toString();
                                c.theme_properties.emplace_back(prop2);
                            } else {
                                qCritical() << "Invalid tag in doc file: " + name3 + ".";
                                return ERR_FILE_CORRUPT;
                            }
                        }
                        else if (parser.tokenType() == QXmlStreamReader::Characters) {
                            if(in_theme_item)
                                c.theme_properties.back().description = parser.text().toString();

                        }
                        else if (parser.tokenType() == QXmlStreamReader::EndElement) {
                            in_theme_item  = false;
                            if(parser.name() == "theme_items")
                                break; // End of <theme_items>.
                        }
                    }

                } else if (name2 == "constants") {
                    bool in_item=false;
                    while (parser.readNext() != QXmlStreamReader::Invalid) {

                        if (parser.tokenType() == QXmlStreamReader::StartElement) {

                            const QStringRef name3 = parser.name();

                            if (name3 == "constant") {
                                in_item=true;
                                const auto attrs(parser.attributes());

                                DocContents::ConstantDoc constant2;
                                ERR_FAIL_COND_V(!attrs.hasAttribute("name"), ERR_FILE_CORRUPT);
                                constant2.name = attrs.value("name").toString();
                                ERR_FAIL_COND_V(!attrs.hasAttribute("value"), ERR_FILE_CORRUPT);
                                constant2.value = attrs.value("value").toString();
                                if (attrs.hasAttribute("enum")) {
                                    constant2.enumeration = attrs.value("enum").toString();
                                }
                                c.constants.push_back(constant2);
                            } else {
                                qCritical() << "Invalid tag in doc file: " + name3 + ".";
                                return ERR_FILE_CORRUPT;
                            }

                        } else if (parser.tokenType() == QXmlStreamReader::Characters) {
                            if(in_item)
                                c.constants.back().description = parser.text().toString();

                        } else if (parser.tokenType() == QXmlStreamReader::EndElement) {
                            in_item  = false;
                            if(parser.name() == "constants")
                                break; // End of <constants>.
                        }
                    }

                } else {
                    qCritical() << "Invalid tag in doc file: " + name2 + ".";
                    return ERR_FILE_CORRUPT;
                }

            } else if (parser.tokenType() == QXmlStreamReader::EndElement && parser.name() == "class")
                break; // End of <class>.
        }
    }

    return OK;
}

Error DocData::save_classes(QByteArray p_default_path, const char *version_branch, const HashMap<String, QString> &p_class_path) {

    for (ClassDoc &c : class_list) {

        QString save_path;
        if (p_class_path.contains_as(qPrintable(c.name))) {
            save_path = p_class_path.at(qPrintable(c.name));
        } else {
            save_path = p_default_path;
        }

        QString save_file = save_path+"/"+c.name + ".xml";
        QFile f(save_file);
        if(!f.open(QFile::WriteOnly)) {
            qWarning()<<"Can't write doc file: " + save_file + ".";
            continue;
        }
        QXmlStreamWriter writer(&f);
        writer.writeStartDocument();
        writer.writeStartElement("class");
        writer.writeAttribute("name",c.name);
        if (!c.inherits.isEmpty())
            writer.writeAttribute("inherits",c.inherits);

        writer.writeAttribute("version",version_branch);
        writer.writeTextElement("brief_description",c.brief_description.trimmed());
        writer.writeTextElement("description",c.description.trimmed());

        writer.writeStartElement("tutorials");

        for (size_t i = 0; i < c.tutorials.size(); i++) {
            writer.writeTextElement("link",c.tutorials[i].trimmed());
        }
        writer.writeEndElement();

        writer.writeStartElement("methods");

        eastl::sort(c.methods.begin(),c.methods.end());

        for (int i = 0; i < c.methods.size(); i++) {

            const MethodDoc &m = c.methods[i];
            writer.writeStartElement("method");

            String qualifiers;

            writer.writeAttribute("name",m.name);
            if (!m.qualifiers.isEmpty())
                writer.writeAttribute("qualifiers",m.qualifiers);

            if (!m.return_type.isEmpty()) {
                writer.writeStartElement("return");
                writer.writeAttribute("type",m.return_type);
                String enum_text;
                if (!m.return_enum.isEmpty()) {
                    writer.writeAttribute("enum",m.return_enum);
                }
                writer.writeEndElement();
            }

            for (int j = 0; j < m.arguments.size(); j++) {

                const ArgumentDoc &a = m.arguments[j];
                writer.writeStartElement("argument");

                writer.writeAttribute("index",QString::number(j));
                writer.writeAttribute("name",a.name);
                writer.writeAttribute("type",a.type);
                if(!a.enumeration.isEmpty())
                    writer.writeAttribute("enum",a.enumeration);
                if(!a.default_value.isEmpty())
                    writer.writeAttribute("default",a.default_value);

                writer.writeEndElement();
            }
            writer.writeTextElement("description",m.description.trimmed());

            writer.writeEndElement();
        }

        writer.writeEndElement();

        if (!c.properties.empty()) {
            writer.writeStartElement("methods");

            eastl::sort(c.properties.begin(),c.properties.end());

            for (size_t i = 0; i < c.properties.size(); i++) {
                writer.writeStartElement("member");
                const PropertyDoc &a(c.properties[i]);

                writer.writeAttribute("name",a.name);
                writer.writeAttribute("type",a.type);
                writer.writeAttribute("setter",a.setter);
                writer.writeAttribute("getter",a.getter);
                if (c.properties[i].overridden)
                    writer.writeAttribute("overridden","true");

                if (!c.properties[i].enumeration.isEmpty())
                    writer.writeAttribute("enum",a.enumeration);

                if (!c.properties[i].default_value.isEmpty())
                    writer.writeAttribute("default",a.default_value);
                if (!c.properties[i].overridden)
                    writer.writeCharacters(a.description);

                writer.writeEndElement();
            }
            writer.writeEndElement();
        }

        if (!c.defined_signals.empty()) {

            eastl::sort(c.defined_signals.begin(),c.defined_signals.end());

            writer.writeStartElement("signals");
            for (size_t i = 0; i < c.defined_signals.size(); i++) {

                const MethodDoc &m = c.defined_signals[i];
                writer.writeStartElement("signal");
                writer.writeAttribute("name",m.name);

                for (int j = 0; j < m.arguments.size(); j++) {
                    const ArgumentDoc &a = m.arguments[j];
                    writer.writeStartElement("argument");
                        writer.writeAttribute("index",QString::number(j));
                        writer.writeAttribute("name",a.name);
                        writer.writeAttribute("type",a.type.trimmed());
                    writer.writeEndElement();
                }
                writer.writeTextElement("description",m.description.trimmed());

                writer.writeEndElement();
            }

            writer.writeEndElement();
        }
        writer.writeStartElement("constants");

        for (int i = 0; i < c.constants.size(); i++) {
            const ConstantDoc &k = c.constants[i];
            writer.writeStartElement("constant");
                writer.writeAttribute("name",k.name);
                writer.writeAttribute("value",k.value);
                if (!k.enumeration.isEmpty())
                    writer.writeAttribute("enumeration",k.enumeration);
                writer.writeCharacters(k.description.trimmed());
            writer.writeEndElement();
        }

        writer.writeEndElement();

        if (!c.theme_properties.empty()) {

            eastl::sort(c.theme_properties.begin(),c.theme_properties.end());

            writer.writeStartElement("theme_items");
            for (size_t i = 0; i < c.theme_properties.size(); i++) {

                const PropertyDoc &p = c.theme_properties[i];
                writer.writeStartElement("theme_item");
                writer.writeAttribute("name",p.name);
                writer.writeAttribute("type",p.type);
                if(!p.default_value.isEmpty())
                    writer.writeAttribute("default_value",p.default_value);

                writer.writeCharacters(p.description);

                writer.writeEndElement();
            }
            writer.writeEndElement();
        }

        writer.writeEndElement();
    }

    return OK;
}
inline QByteArray uncompr_zip(const char *comp_data,int size_comprs,uint32_t size_uncom)
{
    QByteArray compressed_data;
    compressed_data.reserve(size_comprs+4);
    compressed_data.append( char((size_uncom >> 24) & 0xFF));
    compressed_data.append( char((size_uncom >> 16) & 0xFF));
    compressed_data.append( char((size_uncom >> 8) & 0xFF));
    compressed_data.append( char((size_uncom >> 0) & 0xFF));
    compressed_data.append(comp_data,size_comprs);
    return qUncompress(compressed_data);
}
Error DocData::load_compressed(const uint8_t *p_data, int p_compressed_size, int p_uncompressed_size) {

    QByteArray data;
    data.resize(p_uncompressed_size);
    data = uncompr_zip((const char *)p_data,p_compressed_size,p_uncompressed_size);

    class_list.clear();

    QXmlStreamReader xml_reader(data);

    _load(xml_reader,*this);

    return OK;
}
