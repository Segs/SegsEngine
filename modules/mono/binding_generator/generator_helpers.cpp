#include "generator_helpers.h"
#include "type_system.h"

#include "core/string_utils.h"
#include "core/string_builder.h"

#include "EASTL/vector_set.h"

#include <QDebug>
#include <QFile>

void _err_print_error(const char* p_function, const char* p_file, int p_line, StringView p_error, StringView p_message, ErrorHandlerType p_type) {

    qWarning() << QLatin1String(p_error.data(), p_error.size());
    qWarning() << QLatin1String(p_message.data(), p_message.size());
}
int Vsnprintf8(char* pDestination, size_t n, const char* pFormat, va_list arguments)
{
    #ifdef _MSC_VER
        return _vsnprintf(pDestination, n, pFormat, arguments);
    #else
        return vsnprintf(pDestination, n, pFormat, arguments);
    #endif
}
void print_line(StringView p_string) {
    qDebug()<< String(p_string).c_str();
}
void print_verbose(StringView p_string) {
    qDebug() << "V: "<<String(p_string).c_str();
}
String itos(int64_t v) {
    char buf[32];
    snprintf(buf,31,"%lld",v);
    return buf;
}

String c_func_name_to_cs(StringView method_name) {
    String class_name;
    String namespace_name;
    String proxy_name = escape_csharp_keyword(snake_to_pascal_case(method_name));
    String mapped_class_name; // = mapClassName(class_name, namespace_name)

    // Prevent the method and its enclosing type from sharing the same name
    if ((!class_name.empty() && proxy_name == mapped_class_name) || (!namespace_name.empty() && proxy_name==namespace_name)) {
        qWarning("Name of method '%s' is ambiguous with the name of its enclosing class '%s'. Renaming method to '%s_'\n",
                 proxy_name.c_str(), mapped_class_name.c_str(), String(method_name).c_str());

        proxy_name += "_";
    }
    return proxy_name;
}

String c_property_name_to_cs(StringView method_name) {
    String class_name;
    String namespace_name;
    String proxy_name = escape_csharp_keyword(snake_to_pascal_case(method_name));
    String mapped_class_name; // = mapClassName(class_name, namespace_name)

    // Prevent the method and its enclosing type from sharing the same name
    if ((!class_name.empty() && proxy_name == mapped_class_name) || (!namespace_name.empty() && proxy_name==namespace_name)) {
        qWarning("Name of method '%s' is ambiguous with the name of its enclosing class '%s'. Renaming method to '%s_'\n",
                 proxy_name.c_str(), mapped_class_name.c_str(), String(method_name).c_str());

        proxy_name += "_";
    }
    // The passed property name could have been taken from a group or array description, so we just remove spaces.
    proxy_name.replace(" ","");
    return proxy_name;
}

static bool is_csharp_keyword(StringView p_name) {
    using namespace eastl;
    static vector_set<StringView, eastl::less<StringView>, EASTLAllocatorType, eastl::fixed_vector<StringView, 79, false>>
        keywords;
    static bool initialized = false;
    if (!initialized) {
        constexpr const char* kwords[] = {
            "abstract" ,"as" ,"base" ,"bool" ,
            "break" ,"byte" ,"case" ,"catch" ,
            "char" ,"checked" ,"class" ,"const" ,
            "continue" ,"decimal" ,"default" ,"delegate" ,
            "do" ,"double" ,"else" ,"enum" ,
            "event" ,"explicit" ,"extern" ,"false" ,
            "finally" ,"fixed" ,"float" ,"for" ,
            "foreach" ,"goto" ,"if" ,"implicit" ,
            "in" ,"int" ,"interface" ,"internal" ,
            "is" ,"lock" ,"long" ,"namespace" ,
            "new" ,"null" ,"object" ,"operator" ,
            "out" ,"override" ,"params" ,"private" ,
            "protected" ,"public" ,"readonly" ,"ref" ,
            "return" ,"sbyte" ,"sealed" ,"short" ,
            "sizeof" ,"stackalloc" ,"static" ,"string" ,
            "struct" ,"switch" ,"this" ,"throw" ,
            "true" ,"try" ,"typeof" ,"uint" ,"ulong" ,
            "unchecked" ,"unsafe" ,"ushort" ,"using" ,
            "value", // contextual kw.
            "virtual" ,"volatile" ,"void" ,"while"
        };
        for (const char* c : kwords)
            keywords.emplace(c);
        initialized = true;
    }
    // Reserved keywords
    return keywords.contains(p_name);
}

String escape_csharp_keyword(StringView p_name) {
    return is_csharp_keyword(p_name) ? String("_") + p_name : String(p_name);
}

bool allUpperCase(StringView s) {
    for(char c : s) {
        if(eastl::CharToUpper(c)!=c)
            return false;
    }
    return true;
}

String snake_to_pascal_case(StringView p_identifier, bool p_input_is_upper) {

    String ret;
    Vector<StringView> parts;
    String::split_ref(parts,p_identifier, "_", true);

    for (size_t i = 0; i < parts.size(); i++) {
        String part(parts[i]);

        if (part.length()) {
            part[0] = eastl::CharToUpper(part[0]);
            if (p_input_is_upper) {
                for (size_t j = 1; j < part.length(); j++)
                    part[j] = eastl::CharToLower(part[j]);
            }
            ret += part;
        }
        else {
            if (i == 0 || i == (parts.size() - 1)) {
                // Preserve underscores at the beginning and end
                ret += "_";
            }
            else {
                // Preserve contiguous underscores
                if (parts[i - 1].length()) {
                    ret += "__";
                }
                else {
                    ret += "_";
                }
            }
        }
    }

    return ret;
}

void GeneratorContext::append_line(const char *ln) {
    out.append_indented(ln);
    out.append("\n");
}
void GeneratorContext::append_multiline(StringView ln) {
    out.append_indented_multiline(ln);
}

void GeneratorContext::append_line(const String &ln) {
    out.append_indented(ln);
    out.append("\n");
}

void GeneratorContext::start_struct_block(const char *access_level, const String &name) {
    out.append_indented(access_level);
    out.append(" struct ");
    out.append(name);
    out.append("\n");
    out.append_indented("{\n");
    out.indent();
}

void GeneratorContext::start_class_block(const char *access_level, const String &name) {
    out.append_indented(access_level);
    out.append(" class ");
    out.append(name);
    out.append("\n");
    out.append_indented("{\n");
    out.indent();
}

void GeneratorContext::end_block(StringView comment) {
    out.dedent();
    if(!comment.empty()) {
        out.append_indented("} //");
        out.append(comment);
        out.append("\n");
    }
    else
        out.append_indented("}\n");

}

void GeneratorContext::start_cs_namespace(StringView name) {
    out.append_indented("namespace ");
    out.append(name);
    out.append("\n");
    start_block();
}

void GeneratorContext::start_block() {
    out.append_indented("{\n");
    out.indent();
}

String c_func_name_to_icall(const TS_Function *fn) {
    String res="icall_";
    if(fn->enclosing_type) {
        res.append(fn->enclosing_type->c_name());
        res.append("_");
    }
    res+=fn->c_name();
    return res;
}
String c_special_func_name_to_icall(const TS_TypeLike *fn,SpecialFuncType kind) {
    String res="icall_";
    if(fn) {
        res.append(fn->c_name());
        res.append("_");
    }
    switch(kind) {
    case Constructor:
        res+="Ctor";
        break;
    case Singleton:
        res+="get_singleton";
        break;
    default:
        assert(false);
    }

    return res;
}
void write_gen(const GeneratorContext &ctx,QFile &tgt) {
    String cs_str=ctx.out.as_string();
    tgt.write(cs_str.c_str(),cs_str.size());

}

void ProjectContext::set_generator(GeneratorContext *tgt) {
    m_cs_impls = tgt;
}

ProjectContext::ProjectContext(const ReflectionData &rd, QString tgt_dir) :
    m_base_path(tgt_dir),
    m_target_dir(tgt_dir),
    m_rd(rd)
{

}

GeneratorContext *ProjectContext::add_source_file(const String &fname)
{
    GeneratorContext * res = new GeneratorContext;
    res->tgt_file_path=fname;
    set_generator(res);

    return res;
}

void ProjectContext::enter_subdir(const String &type_path)
{
    create_and_cd_to_gen_dir(type_path.c_str());
    m_base_path = m_target_dir.path();
}
void ProjectContext::leave_subdir() {
    m_target_dir.cdUp();
    m_base_path = m_target_dir.path();
}

bool ProjectContext::create_and_cd_to_gen_dir(QString gen_dir) {
    m_target_dir = QDir(m_base_path);
    if(!m_target_dir.cd(gen_dir)) {
        bool mk_ok = m_target_dir.mkpath(gen_dir);
        if(!mk_ok) {
            qFatal("Failed to create target directory");
            return false;
        }
    }
    return m_target_dir.cd(gen_dir);
}
bool ProjectContext::write_string_builder(StringView partial_target_path,const StringBuilder &str)
{
    return write_string(partial_target_path,str.as_string());
}

bool ProjectContext::write_string(StringView partial_target_path, StringView str)
{
    QString tgt_pathname(QString::fromLatin1(partial_target_path.data(),partial_target_path.size()));

    m_target_dir.setPath(m_base_path);
    if(!m_target_dir.mkpath(QFileInfo(tgt_pathname).path()))
    {
        qCritical() << "Failed to create tgt path";
        return false;
    }
    QFile icall_cpp(m_target_dir.filePath(tgt_pathname));
    icall_cpp.open(QFile::WriteOnly);
    icall_cpp.write(str.data(),str.size());
    icall_cpp.close();
    return true;
}

//bool ProjectContext::write_files()
//{
//    QFile icall_cs;
//    QFile cs_impl;
//    create_and_cd_to_gen_dir(".");

//    m_target_dir.setPath(m_base_path);

//    create_and_cd_to_gen_dir("cs");
//    for(const auto &entry : m_cs_impls) {
//        cs_impl.setFileName(m_target_dir.filePath(entry.first.c_str()));
//        cs_impl.open(QFile::WriteOnly);
//        write_gen(entry.second,cs_impl);
//        cs_impl.close();
//    }
//    m_target_dir.setPath(m_base_path);

//    icall_cs.setFileName(m_target_dir.filePath("icall.cs"));
//    icall_cs.open(QFile::WriteOnly);
//    write_gen(icall_cs_impl_ctx(),icall_cs);
//    icall_cs.close();

//    return true;
//}
