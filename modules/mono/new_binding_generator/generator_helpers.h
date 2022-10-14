#pragma once
#include "core/forward_decls.h"
#include "core/string.h"
#include "core/string_builder.h"
#include "core/map.h"

#include <QDir>
constexpr int build_version_number(int major,int minor,int patch) {
    return major*1000+minor*10+patch;
}
constexpr int g_generator_version=build_version_number(4,0,0);

struct ReflectionData;
struct TS_TypeMapper;
struct TS_Function;
struct TS_TypeLike;

//StringView method_name, StringView class_name, StringView namespace_name
String c_func_name_to_cs(StringView method_name);
String c_property_name_to_cs(StringView method_name);
String snake_to_pascal_case(StringView p_identifier, bool p_input_is_upper = false);
String escape_csharp_keyword(StringView p_name);
String c_func_name_to_icall(const TS_Function *fn);
enum SpecialFuncType {
    Constructor,
    Singleton,
};

String c_special_func_name_to_icall(const TS_TypeLike *fn, SpecialFuncType kind);
bool allUpperCase(StringView s);

struct GeneratorContext {
    StringBuilder out;
    String tgt_file_path;
    GeneratorContext() { }
    void append_line(const char *ln);
    void append_line(const String &ln);
    void append_multiline(StringView ln);

    void start_struct_block(const char *access_level, const String &name);
    void start_class_block(const char *access_level, const String &name);
    void start_block();
    void start_cs_namespace(StringView name);

    void end_block(StringView comment="");

};

struct ProjectContext {
public:
    QString m_base_path;
    QDir m_target_dir;
    const ReflectionData &m_rd;
    GeneratorContext &generator() {
        assert(m_cs_impls);
        return *m_cs_impls;
    }
    void set_generator(GeneratorContext *tgt);
    //GeneratorContext &icall_cpp_impl_ctx();

    ProjectContext(const ReflectionData &rd,QString tgt_dir);

    GeneratorContext *add_source_file(const String &path_and_name);

    void enter_subdir(const String &type_path);
    void leave_subdir();

    bool write_string_builder(StringView partial_target_path, const StringBuilder &str);
    bool write_string(StringView partial_target_path, StringView str);
private:
    bool create_and_cd_to_gen_dir(QString gen_dir);
private:
    Vector<String> m_source_files;
    GeneratorContext *m_cs_impls = nullptr;
};

