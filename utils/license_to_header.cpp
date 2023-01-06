#include <QtCore/QFile>
#include <QtCore/QString>
#include <QFileInfo>
#include <QStringList>
#include <QHash>
#include <QCryptographicHash>
#include <QTextStream>
#include <QDebug>
#include <QSet>
#include <QJsonDocument>
#include <QMap>
#include <QDirIterator>
#include <QRegularExpression>
#include <QtCore/QCoreApplication>
#include <QtCore/QJsonArray>
#include <QJsonObject>
#include <cassert>
#ifdef _MSC_VER
#include <iso646.h>
#endif
#include <QDateTime>
#include <cstdio>

struct TpEntry
{
    QString comment;
    struct Entry {
        QString tp_file, tp_copyright, tp_license;
    };
    QVector<Entry> entries;
};
QString escape_string(const QString &input)
{
    QString result;
    for(QChar c : input)
        if (!c.isPrint() || c == '\\' || c=='"')
            result += QString("\\%1").arg(uint8_t(c.toLatin1()),3,8,QChar('0'));
        else
            result += c;
    return result;
}

class LicenseReader
{
public:
    QString current;
    QFile &_license_file;
    int line_num = 0;
    LicenseReader(QFile &license_file) : _license_file(license_file)
    {
        current = next_line();
    }
    QString next_line()
    {
        QString line = _license_file.readLine();
        line_num += 1;
        while(line.startsWith("#"))
        {
            line = _license_file.readLine();
            line_num += 1;
        }
        current = line;
        return line;
    }
    std::pair<QString,QStringList> next_tag()
    {
        if (not this->current.contains(':'))
            return {"",{}};
        QStringList parts = current.split(":");
        assert(parts.size()>1);
        QString tag = parts.takeFirst();
        QStringList lines = { parts.join(':').trimmed() };
        while((!next_line().isEmpty()) && current.startsWith(" "))
            lines.append(current.trimmed());
        return {tag, lines};
    }
};
bool make_license_header(const QStringList &source)
{
    QString src_copyright = QFileInfo(source[0]).absoluteFilePath();
    QString src_license = QFileInfo(source[1]).absoluteFilePath();
    QString dst = QFileInfo(source[2]).absoluteFilePath();
    QFile license_file(src_license);
    QFile copyright_file(src_copyright);
    QFile g(dst);
    if(!license_file.open(QFile::ReadOnly) || !copyright_file.open(QFile::ReadOnly) || !g.open(QFile::WriteOnly))
        return false;
    QTextStream out(&g);
    out.setGenerateByteOrderMark(true);
    out.setCodec("UTF-8");
    QMap<QString,QVector<QHash<QString,QStringList>>> projects;
    QVector<QStringList> license_list;

    LicenseReader reader(copyright_file);
    QHash<QString,QStringList> part = {};
    QHash<QString,QStringList> *tgt_part = &part;
    QStringList file_license_copyright_tags= {"Files", "Copyright", "License"};
    while(!reader.current.isEmpty())
    {
        std::pair<QString,QStringList> tag_content = reader.next_tag();
        if(file_license_copyright_tags.contains(tag_content.first))
        {
            (*tgt_part)[tag_content.first] = tag_content.second;
        }
        else if (tag_content.first == "Comment")
        {
            // attach part to named project
            projects[tag_content.second[0]].append(part);
            tgt_part  = &projects[tag_content.second[0]].back();
        }
        if (tag_content.first.isEmpty() or reader.current.isEmpty())
        {
            // end of a paragraph start a new part
            if (tgt_part->contains("License") and not tgt_part->contains("Files"))
            {
                // no Files tag in this one, so assume standalone license
                license_list.append(part["License"]);
            }
            tgt_part = &part;
            part.clear();
            reader.next_line();
        }
    }
    QStringList data_list;
    for(auto &project : projects)
    {
        for(auto &part : project)
        {
            part["file_index"].append(QString::number(data_list.size()));
            data_list.append(part["Files"]);
            part["copyright_index"].append(QString::number(data_list.size()));
            data_list.append(part["Copyright"]);
        }
    }
    out << "/* THIS FILE IS GENERATED DO NOT EDIT */\n";
    out << "#ifndef _EDITOR_LICENSE_H\n";
    out << "#define _EDITOR_LICENSE_H\n";
    out << "const char *const GODOT_LICENSE_TEXT =";

    QString license_line;
    QTextStream license_stream(&license_file);
    while(license_stream.readLineInto(&license_line))
    {
        out << "\n\t\t\"" << escape_string(license_line.trimmed()) + "\\n\"";
    }
    out <<";\n\n";

    out << "struct ComponentCopyrightPart {\n"
            "\tconst char *license;\n"
            "\tconst char *const *files;\n"
            "\tconst char *const *copyright_statements;\n"
            "\tint file_count;\n"
            "\tint copyright_count;\n"
            "};\n\n";

    out << "struct ComponentCopyright {\n"
            "\tconst char *name;\n"
            "\tconst ComponentCopyrightPart *parts;\n"
            "\tint part_count;\n"
            "};\n\n";

    out << "const char *const COPYRIGHT_INFO_DATA[] = {\n";
    for (const auto& line : data_list)
        out << "\t\"" << escape_string(line) << "\",\n";
    out << "};\n\n";

    out << "const ComponentCopyrightPart COPYRIGHT_PROJECT_PARTS[] = {\n";
    int part_index = 0;
    QMap<QString,int> part_indexes = {};
    for(auto iter=projects.begin(),fin = projects.end(); iter!=fin; ++iter)
    {
        QString project_name = iter.key();
        auto &project(iter.value());
        part_indexes[project_name] = part_index;
        for(const auto &part : project)
        {
            out << "\t{ \"" << escape_string(part["License"].front()) << "\", "
                << "&COPYRIGHT_INFO_DATA[" << part["file_index"].join("") << "], "
                << "&COPYRIGHT_INFO_DATA[" << part["copyright_index"].join("") << "], "
                << part["Files"].size() << ", "
                << part["Copyright"].size()<< " },\n";
            part_index++;
        }
    }
    out << "};\n\n";

    out << "const int COPYRIGHT_INFO_COUNT = " << projects.size() << ";\n";

    out << "const ComponentCopyright COPYRIGHT_INFO[] = {\n";
    for(auto iter=projects.begin(),fin = projects.end(); iter!=fin; ++iter)
    {
        QString project_name = iter.key();
        auto &project(iter.value());
        out << "\t{ \"" << escape_string(project_name) << "\", "
                << "&COPYRIGHT_PROJECT_PARTS[" << QString::number(part_indexes[project_name]) << "], "
                << QString::number(project.size()) << " },\n";
    }
    out << "};\n\n";

    out << "const int LICENSE_COUNT = " << license_list.size() << ";\n";

    out << "const char *const LICENSE_NAMES[] = {\n";
    for (const auto &l : license_list)
        out << "\t\"" << escape_string(l[0]) << "\",\n";
    out << "};\n\n";

    out << "const char *const LICENSE_BODIES[] = {\n\n";
    for(auto & l : license_list)
    {
        for (const auto &line : l.mid(1))
        {
            if(line == ".")
                out << "\t\"\\n\"\n";
            else
                out << "\t\"" << escape_string(line) << "\\n\"\n";
        }
        out << "\t\"\",\n\n";
    }
    out << "};\n\n";

    out << "#endif\n";
    return true;
}
bool make_license_header2(QStringList source)
{
    QString src_copyright = QFileInfo(source[0]).absoluteFilePath();
    QString src_license = QFileInfo(source[1]).absoluteFilePath();
    QString dst = QFileInfo(source[2]).absoluteFilePath();
    QFile f(src_license);
    QFile fc(src_copyright);
    QFile g(dst);
    if(!f.open(QFile::ReadOnly) || !fc.open(QFile::ReadOnly) || !g.open(QFile::WriteOnly))
        return false;

    g.write("/* THIS FILE IS GENERATED DO NOT EDIT */\n");
    g.write("#ifndef _EDITOR_LICENSE_H\n");
    g.write("#define _EDITOR_LICENSE_H\n");
    g.write("static const char *GODOT_LICENSE_TEXT =");
    QTextStream lic_stream(&f);
    QString line;
    while(lic_stream.readLineInto(&line))
    {
        QString escaped_string = escape_string(line.trimmed());
        g.write(qUtf8Printable("\n\t\"" + escaped_string + "\\n\""));
    }

    g.write(";\n");

    int tp_current = 0;
    QString tp_file = "";
    QString tp_comment = "";
    QString tp_copyright = "";
    QString tp_license = "";

    QString tp_licensename = "";
    QString tp_licensebody = "";

    QVector<TpEntry> tp;
    QVector<QPair<QString,QString>> tp_licensetext;

    QTextStream copyright_stream(&fc);
    while(copyright_stream.readLineInto(&line))
    {
        if(line.startsWith("#"))
            continue;

        if(line.startsWith("Files:"))
        {
            tp_file = line.mid(6).trimmed();
            tp_current = 1;
        }
        else if (line.startsWith("Comment:"))
        {
            tp_comment = line.mid(8).trimmed();
            tp_current = 2;
        }
        else if (line.startsWith("Copyright:"))
        {
            tp_copyright = line.mid(10).trimmed();
            tp_current = 3;
        }
        else if (line.startsWith("License:"))
        {
            if (tp_current != 0)
            {
                tp_license = line.mid(8).trimmed();
                tp_current = 4;
            }
            else
            {
                tp_licensename = line.mid(8).trimmed();
                tp_current = 5;
            }
        }
        else if (line.startsWith(" "))
        {
            if (tp_current == 1)
                tp_file += "\n" + line.trimmed();
            else if (tp_current == 3)
                tp_copyright += "\n" + line.trimmed();
            else if (tp_current == 5)
            {
                if (line.trimmed() == ".")
                    tp_licensebody += "\n";
                else
                    tp_licensebody += line.midRef(1);
            }
        }
        else
        {
            if (tp_current != 0)
            {
                if (tp_current == 5)
                {
                    tp_licensetext.append({tp_licensename, tp_licensebody});
                    tp_licensename.clear();
                    tp_licensebody.clear();
                }
                else
                {
                    bool added = false;
                    for(auto &i : tp)
                    {
                        if(i.comment == tp_comment)
                        {
                            i.entries.append(TpEntry::Entry {tp_file, tp_copyright, tp_license});
                            added = true;
                            break;
                        }
                    }
                    if(!added)
                        tp.append({tp_comment,{{tp_file, tp_copyright, tp_license}}});

                    tp_file.clear();
                    tp_comment.clear();
                    tp_copyright.clear();
                    tp_license.clear();
                }
                tp_current = 0;
            }
        }
    }
    tp_licensetext.push_back({ tp_licensename, tp_licensebody });

    QString about_thirdparty = "";
    QString about_tp_copyright_count = "";
    QString about_tp_license = "";
    QString about_tp_copyright = "";
    QString about_tp_file = "";

    for(auto i : tp)
    {
        about_thirdparty += "\t\"" + i.comment + "\",\n";
        about_tp_copyright_count += QString::number(i.entries.size()) + ", ";
        for(const auto &j :  i.entries)
        {
            QString file_body = "";
            QString copyright_body = "";
            for(auto k : j.tp_file.split("\n"))
            {
                if(!file_body.isEmpty())
                    file_body += "\\n\"\n";
                QString escaped_string = escape_string(k.trimmed());
                file_body += "\t\"" + escaped_string;
            }
            for(auto k : j.tp_copyright.split("\n"))
            {
                if(!copyright_body.isEmpty())
                    copyright_body += "\\n\"\n";
                QString escaped_string = escape_string(k.trimmed());
                copyright_body += "\t\"" + escaped_string;
            }
            about_tp_file += "\t" + file_body + "\",\n";
            about_tp_copyright += "\t" + copyright_body + "\",\n";
            about_tp_license += "\t\"" + j.tp_license + "\",\n";
        }
    }
    QString about_license_name = "";
    QString about_license_body = "";

    for(const QPair<QString,QString> &i : tp_licensetext)
    {
        QString body = "";
        for (auto j : i.second.split("\n"))
        {
            if(!body.isEmpty())
                body += "\\n\"\n";
            QString escaped_string = escape_string(j.trimmed());
            body += "\t\"" + escaped_string;
        }
        about_license_name += "\t\"" + i.first + "\",\n";
        about_license_body += "\t" + body + "\",\n";
    }
    g.write("static const char *about_thirdparty[] = {\n");
    g.write(about_thirdparty.toUtf8());
    g.write("\t0\n");
    g.write("};\n");
    g.write(qPrintable("#define THIRDPARTY_COUNT " + QString::number(tp.size()) + "\n"));

    g.write("static const int about_tp_copyright_count[] = {\n\t");
    g.write(about_tp_copyright_count.toUtf8());
    g.write("0\n};\n");

    g.write("static const char *about_tp_file[] = {\n");
    g.write(about_tp_file.toUtf8());
    g.write("\t0\n");
    g.write("};\n");

    g.write("static const char *about_tp_copyright[] = {\n");
    g.write(about_tp_copyright.toUtf8());
    g.write("\tnullptr\n");
    g.write("};\n");

    g.write("static const char *about_tp_license[] = {\n");
    g.write(about_tp_license.toUtf8());
    g.write("\tnullptr\n");
    g.write("};\n");

    g.write("static const char *LICENSE_NAMES[] = {\n");
    g.write(about_license_name.toUtf8());
    g.write("\tnullptr\n");
    g.write("};\n");
    g.write(qPrintable("#define LICENSE_COUNT " + QString::number(tp_licensetext.size()) + "\n"));

    g.write("static const char *LICENSE_BODIES[] = {\n");
    g.write(about_license_body.toUtf8());
    g.write("\tnullptr\n");
    g.write("};\n");

    g.write("#endif\n");

    g.close();
    fc.close();
    f.close();
    return true;
}
static void close_section(QTextStream &g)
{
    g << "\tnullptr\n";
    g << "};\n";
}
bool make_authors_header(const QStringList &source)
{

    QStringList sections = {"Project Founders", "Lead Developer", "Project Manager", "Developers"};
    QStringList sections_id = { "AUTHORS_FOUNDERS", "AUTHORS_LEAD_DEVELOPERS",
                                "AUTHORS_PROJECT_MANAGERS", "AUTHORS_DEVELOPERS" };

    QString src_authors = QFileInfo(source[0]).absoluteFilePath();
    QString dst = QFileInfo(source[1]).absoluteFilePath();
    QFile f(src_authors);
    QFile g(dst);
    if(!f.open(QFile::ReadOnly|QFile::Text) || !g.open(QFile::WriteOnly))
        return false;
    QTextStream out(&g);
    out.setGenerateByteOrderMark(true);
    out.setCodec("UTF-8");
    out << "/* THIS FILE IS GENERATED DO NOT EDIT */\n";
    out << "#ifndef _EDITOR_AUTHORS_H\n";
    out << "#define _EDITOR_AUTHORS_H\n";

    QString current_section = "";
    bool reading = false;

    QString line;
    QTextStream authors_stream(&f);
    authors_stream.setCodec("UTF-8");
    authors_stream.setGenerateByteOrderMark(true);

    while(authors_stream.readLineInto(&line))
    {
        if (reading)
        {
            if(line.startsWith("    "))
            {
                out << "\t\"" << escape_string(line.trimmed()) << "\",\n";
                continue;
            }
        }
        if (line.startsWith("## "))
        {
            if (reading)
            {
                close_section(out);
                reading = false;
            }
            for(int i=0; i<sections.size(); ++i)
            {
                QString section = sections[i];
                if (line.trimmed().endsWith(section))
                {
                    current_section = escape_string(sections_id[i]);
                    reading = true;
                    out<< "static const char *" << current_section << "[] = {\n";
                    break;
                }
            }
        }
    }
    if(reading)
        close_section(out);

    out << "#endif\n";
    return true;
}
bool make_donors_header(QStringList source)
{
    QStringList sections = { "Platinum sponsors", "Gold sponsors", "Silver sponsors", "Bronze sponsors", "Mini sponsors",
                             "Gold donors", "Silver donors", "Bronze donors" };
    QStringList sections_id = { "DONORS_SPONSOR_PLATINUM", "DONORS_SPONSOR_GOLD", "DONORS_SPONSOR_SILVER",
                                "DONORS_SPONSOR_BRONZE", "DONORS_SPONSOR_MINI", "DONORS_GOLD", "DONORS_SILVER", "DONORS_BRONZE" };

    QString src_donors = QFileInfo(source[0]).absoluteFilePath();
    QString dst = QFileInfo(source[1]).absoluteFilePath();
    QFile f(src_donors);
    QFile g(dst);
    if(!f.open(QFile::ReadOnly) || !g.open(QFile::WriteOnly))
        return false;
    QTextStream out(&g);
    out.setGenerateByteOrderMark(true);
    out.setCodec("UTF-8");
    out << "/* THIS FILE IS GENERATED DO NOT EDIT */\n";
    out << "#ifndef _EDITOR_DONORS_H\n";
    out << "#define _EDITOR_DONORS_H\n";

    QString current_section = "";
    bool reading = false;

    QString line;
    QTextStream donors_stream(&f);
    while(donors_stream.readLineInto(&line))
    {
        if(reading)
        {
            if (line.startsWith("    "))
            {
                out << "\t\"" << escape_string(line.trimmed()) << "\",\n";
                continue;
            }
        }
        if (line.startsWith("## "))
        {
            if (reading)
            {
                close_section(out);
                reading = false;
            }
            for(int i=0; i < sections.size(); ++i)
            {
                if (line.trimmed().endsWith(sections[i]))
                {
                    current_section = escape_string(sections_id[i]);
                    reading = true;
                    out << "static const char *" << current_section << "[] = {\n";
                    break;
                }
            }
        }
    }
    if (reading)
        close_section(out);

    out << "#endif\n";

    return true;
}
static bool _make_doc_data_class_path(const QStringList &paths,const QString &to_path)
{
    QFile g(to_path+"/doc_data_class_path.gen.h");
    if(!g.open(QFile::WriteOnly))
        return false;
    QStringList sorted;
    sorted.reserve(100);
    for(const QString &path : paths)
    {
        if(path.contains("doc/classes")) // skip engine docs
            continue;
        sorted.push_back(path);
    }

    std::sort(sorted.begin(),sorted.end());

    g.write(qPrintable("static const int _doc_data_class_path_count = " + QString::number(sorted.size()) + ";\n"));
    g.write("struct _DocDataClassPath { const char* name; const char* path; };\n");

    g.write(qPrintable("static const _DocDataClassPath _doc_data_class_paths[" + QString::number(sorted.size()+1) + "] = {\n"));
    for(const auto &c : sorted)
    {
        QFileInfo fi(c);
        QString module_path = fi.path();
        module_path = module_path.mid(module_path.indexOf("modules"));
        g.write(qUtf8Printable(QString("\t{\"%1\", \"%2\"},\n").arg(fi.baseName(), module_path)));

    }
    g.write("\t{nullptr, nullptr}\n");
    g.write("};\n");
    g.close();
    return true;
}
QStringList collect_docs(const QString &src_path,const QString &tgt_doc_path)
{
    QFile list_fl(src_path);
    if(!list_fl.open(QFile::ReadOnly))
        return {};
    QStringList all_paths = QString(list_fl.readAll()).split(';');
    QStringList docs;

    for(const auto &path : all_paths)
    {
        if(path.isEmpty())
            continue;

        QDirIterator dir_iter(path.trimmed(),QDir::Files,QDirIterator::Subdirectories);
        while(dir_iter.hasNext())
        {
            docs.push_back(dir_iter.next());
        }
    }

    _make_doc_data_class_path(docs,tgt_doc_path);

//    docs = sorted(docs)
    return docs;
}
static void byteArrayToHexInFile(const QByteArray &src,QFile &g)
{
    int column_count=0;
    for(char b : src)
    {
        if(column_count==0) {
            g.write("\t");
        }
        g.write(qPrintable(QString("0x%1,").arg(uint16_t(uint8_t(b)),2,16,QChar('0'))));
        column_count++;
        if(column_count==20)
        {
            g.write("\n");
            column_count=0;
        }
    }

}
bool collect_and_pack_docs(QStringList args)
{
    QString doc_paths=args.takeFirst();
    QString tgt_path= args.takeFirst();
    QString dst = tgt_path+"/doc_data_compressed.gen.h";
    QStringList all_doc_paths=collect_docs(doc_paths,tgt_path);
    QFile g(dst);
    if(!g.open(QFile::WriteOnly)) {
        return false;
    }

    QByteArray buf = "";

    for (const QString &s : all_doc_paths)
    {
        if(!s.endsWith(".xml")) {
            continue;
        }
        QString src = QFileInfo(s).absoluteFilePath();
        QFile f(src);
        QByteArray content;
        if(f.open(QFile::ReadOnly))
            content = f.readAll();
        buf.append(content);
    }
    int decomp_size = buf.size();
    QByteArray compressed = qCompress(buf);
    compressed.remove(0,4); // remove the decomp size that is added by qCompress

    g.write("/* THIS FILE IS GENERATED DO NOT EDIT */\n");
    g.write("#ifndef _DOC_DATA_RAW_H\n");
    g.write("#define _DOC_DATA_RAW_H\n");
    g.write(qPrintable("static const int _doc_data_compressed_size = " + QString::number(compressed.size()) + ";\n"));
    g.write(qPrintable("static const int _doc_data_uncompressed_size = " + QString::number(decomp_size) + ";\n"));
    g.write("static const unsigned char _doc_data_compressed[] = {\n");
    byteArrayToHexInFile(compressed,g);
    g.write("};\n");
    g.write("#endif");
    g.close();
    return true;
}
bool make_translations_header(QStringList args)
{
    QString translations_path=args.takeFirst();
    QString tgt_path = args.takeFirst();
    QString dst = tgt_path+"/translations.gen.h";
    QFile g(dst);
    if(!g.open(QFile::WriteOnly)) {
        return false;
    }

    g.write("/* THIS FILE IS GENERATED DO NOT EDIT */\n");
    g.write("#ifndef _EDITOR_TRANSLATIONS_H\n");
    g.write("#define _EDITOR_TRANSLATIONS_H\n");
    QDirIterator translation_files_iter(translations_path,{"*.po"},QDir::Files);
    QStringList all_translation_paths;
    while(translation_files_iter.hasNext())
    {
        all_translation_paths.push_back(translation_files_iter.next());

    }

    std::sort(all_translation_paths.begin(),all_translation_paths.end());

//    paths = [node.srcnode().abspath for node in source]
//    sorted_paths = sorted(paths, key=lambda path: os.path.splitext(os.path.basename(path))[0])
    struct TranslationEntry {
       QString name;
       int comp_len;
       int decomp_len;
    };
    QVector<TranslationEntry> xl_names;
    for(auto path : all_translation_paths)
    {
        QFile fl(path);
        fl.open(QFile::ReadOnly);
        QByteArray buf = fl.readAll();
        auto decomp_size = buf.size();
        buf = qCompress(buf);
        buf.remove(0,4); // remove the decomp size that is added by qCompress
        QString name = QFileInfo(path).baseName();

        g.write(qUtf8Printable("static const unsigned char _translation_" + name + "_compressed[] = {\n"));
        byteArrayToHexInFile(buf,g);
        g.write("};\n");

        xl_names.append({name, buf.size(), decomp_size});
    }
    g.write("struct EditorTranslationList {\n");
    g.write("\tconst char* lang;\n");
    g.write("\tint comp_size;\n");
    g.write("\tint uncomp_size;\n");
    g.write("\tconst unsigned char* data;\n");
    g.write("};\n\n");
    g.write("static EditorTranslationList _editor_translations[] = {\n");
    for (TranslationEntry &x : xl_names)
    {
        g.write(qUtf8Printable(QString("\t{ \"%1\", %2, %3, _translation_%1_compressed},\n")
                           .arg(x.name).arg(x.comp_len).arg(x.decomp_len)));
    }
    g.write("\t{nullptr, 0, 0, nullptr}\n");
    g.write("};\n");
    g.write("#endif");
    g.close();
    return true;
}
bool make_default_controller_mappings(QStringList args)
{
    QString dst = args.takeFirst();
    QFile g(dst);
    QDir d;
    d.mkpath(QFileInfo(dst).path());
    if(!g.open(QFile::WriteOnly)) {
        return false;
    }
    g.write("/* THIS FILE IS GENERATED DO NOT EDIT */\n");
    g.write("#include \"core/input/default_controller_mappings.h\"\n");

    // ensure mappings have a consistent order
    QMap<QString,QMap<QString,QString>> platform_mappings;
    for(const QString &src : args)
    {
        QString src_path = QFileInfo(src).absoluteFilePath();
        QFile f(src_path);
        QStringList mapping_file_lines;
        if(f.open(QFile::ReadOnly))
        {
            QTextStream inp_text_stream(&f);
            //# read mapping file and skip header
            QString z;
            while(inp_text_stream.readLineInto(&z))
                mapping_file_lines.push_back(z);
            mapping_file_lines = mapping_file_lines.mid(2);
        }
        QString current_platform;
        for(QString line : mapping_file_lines)
        {
            if(line.isEmpty()) {
                continue;
            }
            line = line.trimmed();
            if(line.isEmpty()) {
                continue;
            }
            if (line[0] == "#")
            {
                current_platform = line.mid(1).trimmed();
            }
            else if (!current_platform.isEmpty())
            {
                QStringList line_parts = line.split(',');
                QString guid = line_parts[0];
                if(platform_mappings[current_platform].contains(guid))
                    g.write(qPrintable(
                        QString("// WARNING - DATABASE %1 OVERWROTE PRIOR MAPPING: %2 %3\n").arg(src_path, current_platform, platform_mappings[current_platform][guid])));
                bool valid_mapping = true;
                for(const auto &input_map : line_parts.mid(2))
                {
                    if(input_map.contains("+") or input_map.contains("-") or input_map.contains("~") )
                    {
                        g.write(qPrintable(
                            QString("// WARNING - DISCARDED UNSUPPORTED MAPPING TYPE FROM DATABASE %1: %2 %3\n").arg(src_path, current_platform, line)));
                        valid_mapping = false;
                        break;
                    }
                }
                if (valid_mapping)
                    platform_mappings[current_platform][guid] = line;
            }
        }
    }
    QMap<QString,QString> platform_variables = {
        {"Linux", "#if X11_ENABLED"},
        {"Windows", "#ifdef WINDOWS_ENABLED"},
        {"Mac OS X", "#ifdef OSX_ENABLED"},
        {"Android", "#if defined(__ANDROID__)"},
        {"iOS", "#ifdef IPHONE_ENABLED"},
        {"Javascript", "#ifdef JAVASCRIPT_ENABLED"},
        {"UWP", "#ifdef UWP_ENABLED"},
    };

    g.write("const char* DefaultControllerMappings::mappings[] = {\n");
    for(auto iter=platform_mappings.begin(),fin=platform_mappings.end(); iter!=fin; ++iter)
    {
        QString variable = platform_variables[iter.key()];
        g.write(qPrintable(variable+"\n"));
        for(auto plat_iter=iter.value().constKeyValueBegin(),plat_fin=iter.value().constKeyValueEnd(); plat_iter!=plat_fin; ++plat_iter)
        {
            g.write(qPrintable(QString("\t\"%1\",\n").arg((*plat_iter).second)));

        }
        g.write("#endif\n");
    }
    g.write("\tnullptr\n};\n");
    g.close();
    return true;
}
bool gen_script_encryption(QStringList args)
{
    QString txt = "0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0";
    QString e = qgetenv("SCRIPT_AES256_ENCRYPTION_KEY");
    if (!e.isEmpty())
    {
        txt = "";
        bool ec_valid = true;
        if (e.size() != 64) {
            ec_valid = false;
        }
        else
        {
            for(int i=0; i<e.size()/2; ++i)
            {
                if (i > 0) {
                    txt += ",";
                }
                bool parse_ok;
                e.midRef(i*2,2).toInt(&parse_ok,16);
                ec_valid &= parse_ok;
                txt += QString("0x%1").arg(e.midRef(i*2,2));
            }
        }
        if (not ec_valid)
        {
            txt = "0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0";
            qCritical()<<"Invalid AES256 encryption key, not 64 bits hex:" << e;
            return false;
        }
    }
    QString target_path = args.takeFirst() + "/script_encryption_key.gen.cpp";
    QFile fl(target_path);
    if(fl.open(QFile::WriteOnly))
    {
        fl.write(qPrintable("#include \"core/project_settings.h\"\nuint8_t script_encryption_key[32]={" + txt + "};\n"));
        return true;
    }
    return false;
}
QString _spaced(QString e)
{
    if(e.endsWith('*')) {
        return e;
    }
    return e + " ";
}
QStringList generate_extension_struct(QString name,const QJsonObject &ext,bool include_version=true)
{
    QStringList ret_val;
    if(ext.contains("next") && ext["next"].isObject())
    {
        ret_val.append(generate_extension_struct(name, ext["next"].toObject()));
    }

    ret_val.append({
        QString("typedef struct godot_gdnative_ext_") + name + (
                       (not include_version) ? "" :
                        QString("_%1_%2").arg(ext["version"]["major"].toInt()).arg(
                                              ext["version"]["minor"].toInt())) + "_api_struct {",
        "\tunsigned int type;",
        "\tgodot_gdnative_api_version version;",
        "\tconst godot_gdnative_api_struct *next;"
    });

    for(const QJsonValue &funcdef : ext["api"].toArray())
    {
        QStringList args;
        for(const QJsonValue &arg_elem : funcdef["arguments"].toArray())
        {
            QJsonArray arg_elems = arg_elem.toArray();
            args.append(QString("%1%2").arg(_spaced(arg_elems.at(0).toString()),arg_elems.at(1).toString()));
        }
        ret_val.append(QString("\t%1(*%2)(%3);").arg(_spaced(funcdef["return_type"].toString()),
                       funcdef["name"].toString(), args.join(",")));
    }
    ret_val += {
            "} godot_gdnative_ext_" + name + ((not include_version) ? QString("") : QString("_%1_%2").arg(ext["version"]["major"].toInt()).arg(ext["version"]["minor"].toInt())) + "_api_struct;",
            ""
    };

    return ret_val;
}
QString _build_gdnative_api_struct_header(QJsonDocument &api)
{
    QStringList gdnative_api_init_macro = {
        "\textern const godot_gdnative_core_api_struct *_gdnative_wrapper_api_struct;"
    };
    QJsonValue extensions = api["extensions"];
    assert(extensions.isArray());
    if(extensions.isArray())
    {
        for(QJsonValue ext : extensions.toArray())
        {
            QString name = ext["name"].toString();
            gdnative_api_init_macro.append(
                QString("\textern const godot_gdnative_ext_%1_api_struct *_gdnative_wrapper_%1_api_struct;").arg(name));
        }

    }

    gdnative_api_init_macro.append("\t_gdnative_wrapper_api_struct = options->api_struct;");
    gdnative_api_init_macro.append("\tfor (unsigned int i = 0; i < _gdnative_wrapper_api_struct->num_extensions; i++) { ");
    gdnative_api_init_macro.append("\t\tswitch (_gdnative_wrapper_api_struct->extensions[i]->type) {");
    for(QJsonValue ext : extensions.toArray())
    {
        QString name = ext["name"].toString();
        QString type = ext["type"].toString();
        gdnative_api_init_macro.append(
            QString("\t\t\tcase GDNATIVE_EXT_%1:").arg(type));
        gdnative_api_init_macro.append(
            QString("\t\t\t\t_gdnative_wrapper_%1_api_struct = (godot_gdnative_ext_%1_api_struct *)"
            " _gdnative_wrapper_api_struct->extensions[i];").arg(name));
        gdnative_api_init_macro.append("\t\t\t\tbreak;");

    }
    gdnative_api_init_macro.append("\t\t}");
    gdnative_api_init_macro.append("\t}");

    QStringList out = {
        "/* THIS FILE IS GENERATED DO NOT EDIT */",
        "#ifndef GODOT_GDNATIVE_API_STRUCT_H",
        "#define GODOT_GDNATIVE_API_STRUCT_H",
        "",
        "#include <gdnative/gdnative.h>",
        "#include <android/godot_android.h>",
        "#include <arvr/godot_arvr.h>",
        "#include <nativescript/godot_nativescript.h>",
        "#include <net/godot_net.h>",
        "#include <pluginscript/godot_pluginscript.h>",
        "#include <videodecoder/godot_videodecoder.h>",
        "",
        "#define GDNATIVE_API_INIT(options) do {  \\\n" + gdnative_api_init_macro.join("  \\\n") + "  \\\n } while (0)",
        "",
        "#ifdef __cplusplus",
        "extern \"C\" {",
        "#endif",
        "",
        "enum GDNATIVE_API_TYPES {",
        "\tGDNATIVE_" + api["core"]["type"].toString() + ','
    };

    for(QJsonValue ext : extensions.toArray())
        out.push_back(QString("\tGDNATIVE_EXT_") + ext["type"].toString() + ',');

    out.push_back("};");
    out.push_back("");


    for(QJsonValue ext : extensions.toArray())
    {
        QString name = ext["name"].toString();
        out.append(generate_extension_struct(name, ext.toObject(), false));
    }

    out.append({
        "typedef struct godot_gdnative_core_api_struct {",
        "\tunsigned int type;",
        "\tgodot_gdnative_api_version version;",
        "\tconst godot_gdnative_api_struct *next;",
        "\tunsigned int num_extensions;",
        "\tconst godot_gdnative_api_struct **extensions;",
    });
    assert(api["core"]["api"].isArray());
    for(const QJsonValue &funcdef : api["core"]["api"].toArray())
    {
        QStringList args;
        for(const QJsonValue &arg_elem : funcdef["arguments"].toArray())
        {
            QJsonArray arg_elems = arg_elem.toArray();
            args.append(QString("%1%2").arg(_spaced(arg_elems.at(0).toString()),arg_elems.at(1).toString()));
        }
        out.append(QString("\t%1(*%2)(%3);").arg(_spaced(funcdef["return_type"].toString()), funcdef["name"].toString(), args.join(',')));
    }
    out.append({
        "} godot_gdnative_core_api_struct;",
        "",
        "#ifdef __cplusplus",
        "}",
        "#endif",
        "",
        "#endif // GODOT_GDNATIVE_API_STRUCT_H",
        ""
    });
    return out.join('\n');
}
static QString get_extension_struct_name(QString name,const QJsonObject &ext, bool include_version=true)
{
    return "godot_gdnative_ext_" + name + ((not include_version)? QString("") :QString("_%1_%2").arg(ext["version"]["major"].toInt()).arg(ext["version"]["minor"].toInt())) + "_api_struct";
};
static QString get_extension_struct_instance_name(QString name,const QJsonObject &ext, bool include_version=true)
{
    return "api_extension_" + name + ((not include_version)? QString("") :QString("_%1_%2").arg(ext["version"]["major"].toInt()).arg(ext["version"]["minor"].toInt())) + "_struct";
}
static QStringList get_extension_struct_definition(QString name,const QJsonObject &ext, bool include_version=true)
{
    QStringList ret_val;

    if(ext.contains("next") && ext["next"].isObject())
    {
        ret_val += get_extension_struct_definition(name, ext["next"].toObject());
    }
    ret_val.append({
        QString("extern const ") + get_extension_struct_name(name, ext, include_version) + ' ' + get_extension_struct_instance_name(name, ext, include_version) + " = {",
        QString("\tGDNATIVE_EXT_") + ext["type"].toString() + ',',
        QString("\t{%1, %2},").arg(ext["version"]["major"].toInt()).arg(ext["version"]["minor"].toInt()),
        QString("\t") + (not ext["next"].isObject() ? "nullptr" :("(const godot_gdnative_api_struct *)&" + get_extension_struct_instance_name(name, ext["next"].toObject()))) + ','
    });

    for(const QJsonValue &funcdef : ext["api"].toArray())
        ret_val.append(QString("\t%1,").arg(funcdef["name"].toString()));
    ret_val.append("};\n");
    return ret_val;
}
QString _build_gdnative_api_struct_source(QJsonDocument &api)
{
    QStringList out = {
        "/* THIS FILE IS GENERATED DO NOT EDIT */",
        "",
        "#include <gdnative_api_struct.gen.h>",
        ""
    };

    for(QJsonValue ext : api["extensions"].toArray())
    {
        QString name = ext["name"].toString();
        out.append(get_extension_struct_definition(name, ext.toObject(), false));
    }
    out.append({"", "const godot_gdnative_api_struct *gdnative_extensions_pointers[] = {"});

    for(QJsonValue ext : api["extensions"].toArray())
    {
        QString name = ext["name"].toString();
        out.append({QString("\t(godot_gdnative_api_struct *)&api_extension_") + name + "_struct,"});
    }
    out.append("};\n");

    out.append({
        QStringLiteral("extern const godot_gdnative_core_api_struct api_struct = {"),
        QString("\tGDNATIVE_") + api["core"]["type"].toString() + ',',
        QString("\t{%1, %2},").arg(api["core"]["version"]["major"].toInt()).arg(api["core"]["version"]["minor"].toInt()),
        QStringLiteral("\tnullptr,"),
        QString("\t%1,").arg(api["extensions"].toArray().size()),
        "\tgdnative_extensions_pointers,",
    });

    for(const QJsonValue &funcdef : api["core"]["api"].toArray())
        out.append(QString("\t%1,").arg(funcdef["name"].toString()));
    out.append("};\n");

    return out.join("\n");
}

bool build_gdnative_api_struct(QStringList args)
{
//    gensource = gdn_env.Command(['include/gdnative_api_struct.gen.h', 'gdnative_api_struct.gen.cpp'],
//                               'gdnative_api.json', build_gdnative_api_struct)
    QString src_json = args.takeFirst();
    QFile json_doc(src_json);
    if(!json_doc.open(QFile::ReadOnly))
        return false;
    QJsonDocument api = QJsonDocument::fromJson(json_doc.readAll());
    QString tgt_dir = args.takeFirst();
    QString header = tgt_dir+"/gdnative_api_struct.gen.h";
    QString source = tgt_dir+"/gdnative_api_struct.gen.cpp";
    QFile header_file(header);
    if(!header_file.open(QFile::WriteOnly))
        return false;
    header_file.write(_build_gdnative_api_struct_header(api).toUtf8());
    QFile src_file(source);
    if(!src_file.open(QFile::WriteOnly))
        return false;
    src_file.write(_build_gdnative_api_struct_source(api).toUtf8());
    return true;
}
bool generate_mono_glue(QStringList args) {
    QString src = args.takeFirst();
    QString dst = args.takeFirst();
    QString version_dst = args.takeFirst();

    QFile header(dst);
    QDir d(".");
    qDebug()<<QFileInfo(dst).path();
    d.mkpath(QFileInfo(dst).path());

    if(!header.open(QFile::WriteOnly)) {
        qCritical("Failed to open destination file");
        return false;
    }
    QDirIterator visitor(src,QDirIterator::Subdirectories);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QStringList files;
    while(visitor.hasNext()) {
        QString fname = visitor.next();
        if(fname.contains("Generated") || fname.contains("obj/"))
            continue;
        if(!fname.endsWith(".cs"))
            continue;
        files.push_back(fname);
    }
    files.sort();
    for(const QString & fname : files) {
        QFile file(fname);
        file.open(QFile::ReadOnly);
        QCryptographicHash hash2(QCryptographicHash::Sha256);
        QByteArray contents = file.readAll();
        // remove end of line chars to normalize hash between windows/linux files.
        contents.replace('\n',"");
        contents.replace('\r',"");
        hash.addData(contents);
        hash2.addData(contents);
        qDebug() << "Hashing"<<fname<<QString::number(qHash(hash.result()),16);
    }
    auto hashed = hash.result();

    auto glue_version = qHash(hashed); // simply hash the array containing sha256
    d.mkpath(QFileInfo(version_dst).path());
    QFile version_header(version_dst);
    if(!version_header.open(QFile::WriteOnly))  {
        qCritical("Failed to open destination file");
        return false;
    }

    version_header.write("/* THIS FILE IS GENERATED DO NOT EDIT */\n");
    version_header.write("#pragma once\n");
    version_header.write(("#define CS_GLUE_VERSION UINT32_C(" + QString::number(glue_version) + ")\n").toLatin1());
    return true;

}
void report_arg_error(const char *mode,int required_args)
{
    qWarning("Not enough arguments for editor_to_header %s mode",mode);
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc,argv);
    if(argc<3)
    {
        qWarning("Not enough arguments for editor_to_header");
        return -1;
    }
    QString mode = app.arguments()[1];
    if(mode=="license")
    {
        if(argc<5)
        {
            report_arg_error("license",argc);
            return -1;
        }
        return make_license_header(app.arguments().mid(2)) ? 0 : -1;
    }
    if(mode=="authors")
    {
        if(argc<4)
        {
            report_arg_error("authors",argc);
            return -1;
        }
        return make_authors_header(app.arguments().mid(2)) ? 0 : -1;
    }
    else if(mode=="donors")
    {
        if(argc<4)
        {
            report_arg_error("donors",argc);
            return -1;
        }
        return make_donors_header(app.arguments().mid(2)) ? 0 : -1;
    }
    else if(mode=="docs")
    {
        if(argc!=4)
        {
            report_arg_error("docs",argc);
            return -1;
        }
        return collect_and_pack_docs(app.arguments().mid(2)) ? 0 : -1;
    }
    else if(mode=="translations")
    {
        if(argc!=4)
        {
            report_arg_error("translations",argc);
            return -1;
        }
        return make_translations_header(app.arguments().mid(2)) ? 0 : -1;
    }
    else if(mode=="controllers")
    {
        if(argc<4)
        {
            report_arg_error("controllers",argc);
            return -1;
        }
        return make_default_controller_mappings(app.arguments().mid(2)) ? 0 : -1;
    }
    else if(mode=="encryption")
    {
        if(argc<3)
        {
            report_arg_error("encryption",argc);
            return -1;
        }
        return gen_script_encryption(app.arguments().mid(2)) ? 0 : -1;
    }
    else if(mode=="gdnative") // exe gdnative json_file target_path
    {
        if(argc!=4)
        {
            report_arg_error("gdnative",argc);
            return -1;
        }
        return build_gdnative_api_struct(app.arguments().mid(2)) ? 0 : -1;
    }
    else if(mode=="mono") {
        if(argc!=5)
        {
            report_arg_error("mono",argc);
            return -1;
        }
        return generate_mono_glue(app.arguments().mid(2)) ? 0 : -1;
    }

}
