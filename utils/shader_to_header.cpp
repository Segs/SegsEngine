#include <cstdio>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QFileInfo>
#include <QStringList>
#include <QHash>
#include <QTextStream>
#include <QDebug>
#include <QSet>
#include <QMap>
#include <QRegularExpression>
#include <QtCore/QCoreApplication>
#ifdef _MSC_VER
#include <iso646.h>
#endif

struct LegacyGLHeaderStruct
{
    QStringList vertex_lines;
    QStringList fragment_lines;
    QSet<QString> uniforms;
    QMap<QString,QString> attributes;
    QMap<QString,QString> feedbacks;
    QStringList fbos;
    QStringList conditionals;
    QMap<QString,QStringList> enums = {};
    QMap<QString,QString> texunits;
    QSet<QString> texunit_names;
    QMap<QString,QString> ubos;
    QSet<QString> ubo_names;

    QSet<QString> vertex_included_files;
    QSet<QString> fragment_included_files;

    QString reading = "";
    int line_offset = 0;
    int vertex_offset = 0;
    int fragment_offset = 0;
};
bool include_file_in_legacygl_header(const QString &filename,LegacyGLHeaderStruct &header_data,int depth)
{
    QTextStream text_input;
    QFile fs(filename);
    if(!fs.open(QFile::ReadOnly))
        return false;
    text_input.setDevice(&fs);
    QString line;
    while(text_input.readLineInto(&line))
    {
        if(line.startsWith("//")) // discard comment lines?
            line = "";

        if (line.contains("[vertex]"))
        {
            header_data.reading = "vertex";
            header_data.line_offset += 1;
            header_data.vertex_offset = header_data.line_offset;
            continue;
        }

        if (line.indexOf("[fragment]") != -1)
        {
            header_data.reading = "fragment";
            header_data.line_offset += 1;
            header_data.fragment_offset = header_data.line_offset;
            continue;
        }

        while(line.indexOf("#include ") != -1)
        {
            QString includeline = QString(line).replace("#include ", "").trimmed();
            includeline = includeline.mid(1,includeline.size()-2);
            QFileInfo file_info(filename);
            QString included_file = file_info.path() + "/" + includeline;
            if (not header_data.vertex_included_files.contains(included_file) and header_data.reading == "vertex")
            {
                header_data.vertex_included_files.insert(included_file);
                if(!include_file_in_legacygl_header(included_file, header_data, depth + 1))
                    qCritical() << QString("Error in file '%1': #include %2 could not be found!").arg(filename,includeline);
            }
            else if (not header_data.fragment_included_files.contains(included_file) and header_data.reading == "fragment")
            {
                header_data.fragment_included_files.insert(included_file);
                if(!include_file_in_legacygl_header(included_file, header_data, depth + 1))
                    qCritical() << QString("Error in file '%1': #include %2 could not be found!").arg(filename,includeline);
            }
            text_input.readLineInto(&line);
        }

        if (line.contains("#ifdef ")) // or line.contains("#elif defined("
        {
            QString ifdefline;
            if (line.indexOf("#ifdef ") != -1)
            {
                ifdefline = QString(line).replace("#ifdef ", "").trimmed();
            }
            else
            {
                ifdefline = QString(line).replace("#elif defined(", "").trimmed();
                ifdefline = ifdefline.replace(")", "").trimmed();
            }

            if (line.indexOf("_EN_") != -1)
            {
                QString enumbase = ifdefline.mid(0,ifdefline.indexOf("_EN_"));
                ifdefline = ifdefline.replace("_EN_", "_");
                line = line.replace("_EN_", "_");
                if( !header_data.enums[enumbase].contains(ifdefline))
                    header_data.enums[enumbase].push_back(ifdefline);
            }
            else
                if(!header_data.conditionals.contains(ifdefline))
                    header_data.conditionals.push_back(ifdefline);
        }
        if (line.contains("uniform") && line.contains("texunit:",Qt::CaseInsensitive))
        {
            // texture unit
            QString texunitstr = line.mid(line.indexOf(":") + 1).trimmed();
            QString texunit;
            if (texunitstr == "auto")
                texunit = "-1";
            else
                texunit = QString::number(texunitstr.toInt());
            QString uline;
			uline = line.mid(0,line.toLower().indexOf("//"));
            uline.replace("uniform", "");
            uline.replace("highp", "");
            uline.replace(";", "");
            QStringList lines = uline.split(',');
            for(auto & x : lines)
            {
                x = x.trimmed();
                x = x.mid(x.lastIndexOf(" ") + 1);
                if (x.indexOf("[") != -1)
                    // uniform array
                    x = x.mid(0,x.indexOf("["));

                if (not header_data.texunit_names.contains(x))
                {
                    header_data.texunits[x] = texunit;
                    header_data.texunit_names.insert(x);
                }
            }
        }
        else if (line.indexOf("uniform") != -1 and line.toLower().indexOf("ubo:") != -1)
        {
            // uniform buffer object
            QString ubostr = line.mid(line.indexOf(":") + 1).trimmed();
            QString ubo = QString::number(ubostr.toInt());
            QString uline = line.mid(0,line.toLower().indexOf("//"));
            uline = uline.mid(uline.indexOf("uniform") + QStringLiteral("uniform").size());
            uline.replace("highp", "");
            uline.replace(";", "");
            uline = uline.replace("{", "").trimmed();
            QStringList lines = uline.split(',');
            for( auto & x : lines)
            {
                x = x.trimmed();
                x = x.mid(x.lastIndexOf(" ") + 1);
                if (x.indexOf("[") != -1)
                {
                    // uniform array
                    x = x.mid(0,x.indexOf("["));
                }

                if (not header_data.ubo_names.contains(x))
                {
                    header_data.ubos[x] = ubo;
                    header_data.ubo_names.insert(x);
                }
            }
        }
        else if (line.indexOf("uniform") != -1 and line.indexOf("{") == -1 and line.indexOf(";") != -1)
        {
            QString uline = QString(line).replace("uniform", "");
            uline.replace(";", "");
            QStringList lines = uline.split(',');
            for( auto & x : lines)
            {
                x = x.trimmed();
                x = x.mid(x.lastIndexOf(" ") + 1);
                if (x.indexOf("[") != -1)
                {
                    // uniform array
                    x = x.mid(0,x.indexOf("["));
                }

                if (not header_data.uniforms.contains(x))
                    header_data.uniforms.insert(x);
            }
        }
        if (line.trimmed().startsWith("attribute ") and line.indexOf("attrib:") != -1)
        {
            QString uline = QString(line).replace("in ", "");
            uline.replace("attribute ", "");
            uline.replace("highp ", "");
            uline.replace(";", "");
            uline = uline.mid(uline.indexOf(" ")).trimmed();
            if (uline.contains("//"))
            {
                QStringList name_bind = uline.split("//");
                QString name = name_bind[0];
                QString bind = name_bind[1];
                if (bind.indexOf("attrib:") != -1)
                {
                        name = name.trimmed();
                        bind = bind.replace("attrib:", "").trimmed();
                        header_data.attributes[name] = bind;
                }
            }
        }
        if (line.trimmed().startsWith("out ") and line.indexOf("tfb:") != -1)
        {
            QString uline = QString(line).replace("out ", "");
            uline.replace("highp ", "");
            uline.replace(";", "");
            uline = uline.mid(uline.indexOf(" ")).trimmed();
            if (uline.contains("//"))
            {
                QStringList name_bind = uline.split("//");
                QString name = name_bind[0];
                QString bind = name_bind[1];
                if (bind.indexOf("tfb:") != -1)
                    header_data.feedbacks[name.trimmed()] = bind.replace("tfb:", "").trimmed();
            }
        }
        line.replace("\r", "");
        line.replace("\n", "");

        if (header_data.reading == "vertex")
            header_data.vertex_lines.push_back(line);
        if (header_data.reading == "fragment")
            header_data.fragment_lines.push_back(line);
        header_data.line_offset += 1;
    }
    fs.close();

    return true;
}
static QString capitalized(const QString &inp)
{
    QStringList parts = inp.split(QRegularExpression("[\\.|_ ]"));
    QString res;
    res.reserve(inp.size());
    for(QString p : parts)
    {
        p[0] = p[0].toUpper();
        res += p;
    }
    return res;
}
void build_legacygl_header(const QString &filename, const char *include, const char *class_suffix,bool output_attribs, bool gles2=false)
{
    LegacyGLHeaderStruct header_data;
    include_file_in_legacygl_header(filename, header_data, 0);

    QString out_file = QFileInfo(filename).fileName() + ".gen.h";
    QFile fdz(out_file);
    qWarning()<<"Creating shader header"<<out_file;

    fdz.open(QFile::WriteOnly);
    QTextStream fd(&fdz);
    QStringList enum_constants;

    fd << "/* WARNING, THIS FILE WAS GENERATED, DO NOT EDIT */\n";

    QString out_file_base = out_file;
    out_file_base = QFileInfo(out_file_base).fileName();
    QString out_file_ifdef = QString(out_file_base).replace(".", "_").toUpper();
    fd << "#pragma once\n";
    QString out_file_class = capitalized(QString(out_file_base).replace(".glsl.gen.h", "")).replace("_", "").replace(".", "") + "Shader" + class_suffix;
    fd << "\n\n";
    fd << QString("#include \"%1\"\n\n\n").arg(include).toLocal8Bit();
    fd << QString("class %1 : public Shader%2 {\n\n").arg(out_file_class,class_suffix).toLocal8Bit();
    fd << "\t virtual String get_shader_name() const { return \"" + out_file_class + "\"; }\n";

    fd << "public:\n\n";

    if (!header_data.conditionals.isEmpty())
    {
        fd << "\tenum Conditionals {\n";
        for(const auto &x : header_data.conditionals)
            fd << "\t\t" + x.toUpper() + ",\n";
        fd << "\t};\n\n";
    }
    if (!header_data.uniforms.isEmpty())
    {
        fd << "\tenum Uniforms {\n";
        for(const auto &x : header_data.uniforms)
            fd << "\t\t" + x.toUpper() + ",\n";
        fd << "\t};\n\n";
    }

    fd << "\t_FORCE_INLINE_ int get_uniform(Uniforms p_uniform) const { return _get_uniform(p_uniform); }\n\n";
    if (!header_data.conditionals.isEmpty())
        fd << "\t_FORCE_INLINE_ void set_conditional(Conditionals p_conditional,bool p_enable)  {  _set_conditional(p_conditional,p_enable); }\n\n";
    fd << R"raw(
    #ifdef DEBUG_ENABLED
    #define _FU if (get_uniform(p_uniform)<0) return; if (!is_version_valid()) return; ERR_FAIL_COND( get_active()!=this );
    #else
    #define _FU if (get_uniform(p_uniform)<0) return;
    #endif
    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, float p_value) { _FU glUniform1f(get_uniform(p_uniform),p_value); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, double p_value) { _FU glUniform1f(get_uniform(p_uniform),p_value); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, uint8_t p_value) { _FU glUniform1i(get_uniform(p_uniform),p_value); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, int8_t p_value) { _FU glUniform1i(get_uniform(p_uniform),p_value); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, uint16_t p_value) { _FU glUniform1i(get_uniform(p_uniform),p_value); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, int16_t p_value) { _FU glUniform1i(get_uniform(p_uniform),p_value); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, uint32_t p_value) { _FU glUniform1i(get_uniform(p_uniform),p_value); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, int32_t p_value) { _FU glUniform1i(get_uniform(p_uniform),p_value); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, const Color& p_color) { _FU GLfloat col[4]={p_color.r,p_color.g,p_color.b,p_color.a}; glUniform4fv(get_uniform(p_uniform),1,col); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, const Vector2& p_vec2) { _FU GLfloat vec2[2]={p_vec2.x,p_vec2.y}; glUniform2fv(get_uniform(p_uniform),1,vec2); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, const Size2i& p_vec2) { _FU GLint vec2[2]={p_vec2.x,p_vec2.y}; glUniform2iv(get_uniform(p_uniform),1,vec2); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, const Vector3& p_vec3) { _FU GLfloat vec3[3]={p_vec3.x,p_vec3.y,p_vec3.z}; glUniform3fv(get_uniform(p_uniform),1,vec3); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, float p_a, float p_b) { _FU glUniform2f(get_uniform(p_uniform),p_a,p_b); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, float p_a, float p_b, float p_c) { _FU glUniform3f(get_uniform(p_uniform),p_a,p_b,p_c); }

    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, float p_a, float p_b, float p_c, float p_d) { _FU glUniform4f(get_uniform(p_uniform),p_a,p_b,p_c,p_d); }

    )raw";

    fd << R"raw(
    _FORCE_INLINE_ void set_uniform(Uniforms p_uniform, const Transform& p_transform) {  _FU
        const Transform &tr = p_transform;

        GLfloat matrix[16]={ /* build a 16x16 matrix */
            tr.basis.elements[0][0],
            tr.basis.elements[1][0],
            tr.basis.elements[2][0],
            0,
            tr.basis.elements[0][1],
            tr.basis.elements[1][1],
            tr.basis.elements[2][1],
            0,
            tr.basis.elements[0][2],
            tr.basis.elements[1][2],
            tr.basis.elements[2][2],
            0,
            tr.origin.x,
            tr.origin.y,
            tr.origin.z,
            1
        };
        glUniformMatrix4fv(get_uniform(p_uniform),1,false,matrix);
    }

    )raw";

    fd << R"raw(_FORCE_INLINE_ void set_uniform(Uniforms p_uniform, const Transform2D& p_transform) {  _FU

        const Transform2D &tr = p_transform;

        GLfloat matrix[16]={ /* build a 16x16 matrix */
            tr.elements[0][0],
            tr.elements[0][1],
            0,
            0,
            tr.elements[1][0],
            tr.elements[1][1],
            0,
            0,
            0,
            0,
            1,
            0,
            tr.elements[2][0],
            tr.elements[2][1],
            0,
            1
        };
        glUniformMatrix4fv(get_uniform(p_uniform),1,false,matrix);
    }
    )raw";

    fd << R"raw(_FORCE_INLINE_ void set_uniform(Uniforms p_uniform, const CameraMatrix& p_matrix) {  _FU
        GLfloat matrix[16];
        for (int i=0;i<4;i++) {
            for (int j=0;j<4;j++) {
                matrix[i*4+j]=p_matrix.matrix[i][j];
            }
        }

        glUniformMatrix4fv(get_uniform(p_uniform),1,false,matrix);
        } )raw";


    fd << "\n\n#undef _FU\n\n\n";

    fd << "\tvirtual void init() {\n\n";

    int enum_value_count = 0;

    if (!header_data.enums.isEmpty())
    {
        fd << "\t\t//Written using math, given nonstandarity of 64 bits integer constants..\n";
        fd << "\t\tstatic const Enum _enums[]={\n";

        auto bitofs = header_data.conditionals.size();
        QVector<QHash<QString,QString>> enum_vals;

        for(QStringList & x : header_data.enums)
        {
            int bits = 1;
            int amt = x.size();
            while((1<<bits) < amt)
                bits += 1;
            QString strs = "{";
            for(int i=0; i<amt; ++i)
            {
                strs += "\"#define " + x[i] + "\\n\",";

                QHash<QString,QString> v = {};
                v["set_mask"] = QString("uint64_t(%1)<<%2)").arg(i).arg(bitofs);
                v["clear_mask"] = QString("((uint64_t(1)<<40)-1) ^ (((uint64_t(1)<<%1) - 1)<<%2)").arg(bits).arg(bitofs);
                enum_vals.append(v);
                enum_constants.append(x[i]);
            }
            strs += "nullptr}";

            fd << QString("\t\t\t{(uint64_t(1<<%1)-1)<<%2,%2,%3},\n").arg(bits).arg(bitofs).arg(strs).toLocal8Bit();
            bitofs += bits;
        }
        fd << "\t\t};\n\n";

        fd << "\t\tstatic const EnumValue _enum_values[]={\n";

        enum_value_count = enum_vals.size();
        for(const auto &x : enum_vals)
            fd << "\t\t\t{" + x["set_mask"] + "," + x["clear_mask"] + "},\n";

        fd << "\t\t};\n\n";
    }
    QStringList conditionals_found;
    if (!header_data.conditionals.isEmpty())
    {
        fd << "\t\tstatic const char* _conditional_strings[]={\n";
        for(const auto &x : header_data.conditionals)
        {
            fd << "\t\t\t\"#define " + x + "\\n\",\n";
            conditionals_found.append(x);
        }
        fd << "\t\t};\n\n";
    }
    else
        fd << "\t\tstatic const char **_conditional_strings=nullptr;\n";

    if (!header_data.uniforms.isEmpty())
    {
        fd << "\t\tstatic const char* _uniform_strings[]={\n";
        for(const auto &x : header_data.uniforms)
            fd << "\t\t\t\"" + x + "\",\n";
        fd<<"\t\t};\n\n";
    }
    else
        fd << "\t\tstatic const char **_uniform_strings=nullptr;\n";

    if(output_attribs)
    {
        if (!header_data.attributes.isEmpty())
        {
            fd<<"\t\tstatic AttributePair _attribute_pairs[]={\n";
            for (auto iter =header_data.attributes.begin(),fin = header_data.attributes.end(); iter!=fin; ++iter)
                fd << "\t\t\t{\"" + iter.key() + "\"," + iter.value() + "},\n";
              fd<<"\t\t};\n\n";
        }
        else
            fd << "\t\tstatic AttributePair *_attribute_pairs=nullptr;\n";
    }
    int feedback_count = 0;

    if (not gles2 and !header_data.feedbacks.isEmpty())
    {
        fd<<"\t\tstatic const Feedback _feedbacks[]={\n";
        for (auto iter =header_data.feedbacks.begin(),fin = header_data.feedbacks.end(); iter!=fin; ++iter)
        {
            const QString &name(iter.key());
            const QString &cond(iter.value());
            if (conditionals_found.contains(cond))
                fd<<"\t\t\t{\"" + name + "\"," + QString::number(conditionals_found.indexOf(cond)) + "},\n";
            else
                fd<<"\t\t\t{\"" + name + "\",-1},\n";

            feedback_count += 1;
        }
        fd<<"\t\t};\n\n";
    }
    else
    {
        if(!gles2)
            fd<<"\t\tstatic const Feedback* _feedbacks=nullptr;\n";
    }
    if (!header_data.texunits.isEmpty())
    {
        fd<<"\t\tstatic TexUnitPair _texunit_pairs[]={\n";
        for (auto iter =header_data.texunits.begin(),fin = header_data.texunits.end(); iter!=fin; ++iter)
            fd<<"\t\t\t{\"" + iter.key() + "\"," + iter.value() + "},\n";
        fd<<"\t\t};\n\n";
    }
    else
        fd<<"\t\tstatic TexUnitPair *_texunit_pairs=nullptr;\n";

    if (not gles2 and !header_data.ubos.isEmpty())
    {
        fd<<"\t\tstatic UBOPair _ubo_pairs[]={\n";
        for (auto iter =header_data.ubos.begin(),fin = header_data.ubos.end(); iter!=fin; ++iter)
            fd<<"\t\t\t{\"" + iter.key() + "\"," + iter.value() + "},\n";
        fd<<"\t\t};\n\n";
    }
    else
    {
        if(!gles2)
            fd<<"\t\tstatic UBOPair *_ubo_pairs=nullptr;\n";
    }

    fd<<"\t\tstatic const char _vertex_code[]={\n";
    for(const QString &x : header_data.vertex_lines)
    {
        for (QChar c : x)
            fd<<QString::number(c.toLatin1()) << ",";
        fd<<QString::number('\n') + ",";
    }
    fd<<"\t\t0};\n\n";

    fd<<"\t\tstatic const int _vertex_code_start=" << header_data.vertex_offset <<";\n";

    fd<<"\t\tstatic const char _fragment_code[]={\n";
    for(const QString &x : header_data.fragment_lines)
    {
        for (QChar c : x)
            fd<<QString::number(c.toLatin1()) << ",";
        fd<<QString::number('\n') + ",";
    }
    fd<<"\t\t0};\n\n";

    fd<<"\t\tstatic const int _fragment_code_start=" << header_data.fragment_offset << ";\n";

    if(output_attribs)
    {
        if(gles2)
            fd<<QString("\t\tsetup(_conditional_strings,%1,_uniform_strings,%2,_attribute_pairs,%3, _texunit_pairs,%4,_vertex_code,_fragment_code,_vertex_code_start,_fragment_code_start);\n").arg(header_data.conditionals.size()).arg(header_data.uniforms.size()).arg(header_data.attributes.size()).arg(header_data.texunits.size());
        else
            fd<<QString("\t\tsetup(_conditional_strings,%1,_uniform_strings,%2,_attribute_pairs,%3, _texunit_pairs,%4,_ubo_pairs,%5,_feedbacks,%6,_vertex_code,_fragment_code,_vertex_code_start,_fragment_code_start);\n").arg(header_data.conditionals.size()).arg(header_data.uniforms.size()).arg(header_data.attributes.size()).arg(header_data.texunits.size()).arg(header_data.ubos.size()).arg(feedback_count);
    }
    else
    {
        if(gles2)
            fd<<QString("\t\tsetup(_conditional_strings,%1,_uniform_strings,%2,_texunit_pairs,%3,_enums,%4,_enum_values,%5,_vertex_code,_fragment_code,_vertex_code_start,_fragment_code_start);\n").
                arg(header_data.conditionals.size()).arg(header_data.uniforms.size()).arg(header_data.texunits.size()).arg(header_data.enums.size()).arg(enum_value_count);
        else
            fd<<QString("\t\tsetup(_conditional_strings,%1,_uniform_strings,%2,_texunit_pairs,%3,_enums,%4,_enum_values,%5,_ubo_pairs,%6,_feedbacks,%7,_vertex_code,_fragment_code,_vertex_code_start,_fragment_code_start);\n").
                    arg(header_data.conditionals.size()).arg(header_data.uniforms.size()).arg(header_data.texunits.size()).arg(header_data.enums.size()).arg(enum_value_count).arg(header_data.ubos.size()).arg(feedback_count);
    }

    fd<<"\t}\n\n";

    if (!enum_constants.isEmpty())
    {
        fd<<"\tenum EnumConditionals {\n";
        for(const auto &x : enum_constants)
            fd<<"\t\t" + x.toUpper() + ",\n";
        fd<<"\t};\n\n";
        fd<<"\tvoid set_enum_conditional(EnumConditionals p_cond) { _set_enum_conditional(p_cond); }\n";
    }
    fd<<"};\n\n";
}
void build_gles3_headers(const QStringList &source)
{
    for(const auto &x : source)
        build_legacygl_header(x, "drivers/gles3/shader_gles3.h", "GLES3", true);
}


void build_gles2_headers(const QStringList &source)
{
    for(const auto &x : source)
        build_legacygl_header(x, "drivers/gles2/shader_gles2.h", "GLES2", true, true);
}


int main(int argc, char **argv)
{
    QCoreApplication app(argc,argv);
    if(argc<3)
    {
        qWarning("Not enough arguments for shader_to_header");
        return -1;
    }
    int gl_ver = app.arguments().at(1).toInt();
    if(gl_ver==2)
        build_gles2_headers(app.arguments().mid(2));
    else if(gl_ver==3)
        build_gles3_headers(app.arguments().mid(2));
    return 0;
}
