#include "generator_helpers.h"
#include "property_generator.h"
#include "type_generator.h"
#include "type_mapper.h"
#include "type_system.h"
#include "cs_generator_visitor.h"
#include "doc_resolution_pass.h"
#include "type_registration_pass.h"

#include "core/deque.h"
#include "core/error_list.h"
#include "core/reflection_support/reflection_data.h"
#include "core/set.h"
#include "core/string_builder.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"

#include "cpp_generator.h"
#include "icall_cs_generator.h"
#include "reflection_visitor_support.h"

#include "EASTL/algorithm.h"
#include "EASTL/sort.h"
#include "EASTL/unordered_set.h"
#include "EASTL/vector_set.h"
#include "core/hash_map.h"
#include "core/hash_set.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStringBuilder>
//get_remote_node -> nodepath
//get_shape_owners -> array

struct TypemapRegistrationPass : public ReflectionVisitorBase {
public:
    explicit TypemapRegistrationPass(ProjectContext &ctx) : ReflectionVisitorBase(ctx) {}

    void visit(const ReflectionData *refl) override
    {
        (void)refl;
        // Does nothing, only registers typemaps for custom types
        TS_TypeMapper::get().register_godot_base_types();
    }
};

static void process(ProjectContext &ctx) {
    ReflectionVisitorBase *passes[] = {
        new TypeRegistrationPass(ctx),
        new TypemapRegistrationPass(ctx),
        new DocResolutionPass(ctx),
        new CppGeneratorVisitor(ctx),
        new CsInterfaceVisitor(ctx),
        new CsGeneratorVisitor(ctx),
    };
    for (auto pass : passes)
        pass->visit(&ctx.m_rd);

    for (auto pass : passes)
        pass->finalize();

    for (auto pass : passes)
        delete pass;

    //    TS_Function tf;
    //    tf.cs_name="nest";
    //    process_call(tf,ctx);

    //    TS_Property pinfo;
    //    pinfo.cs_name = "Proppy";
    //    pinfo.setter = TS_Function {"set_proppy","void",{"int"}};
    //    pinfo.getter = TS_Function {"get_proppy","int",{}};
    //    process_property(pinfo,ctx);

    //    /*
    //        ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, StringName("blend_point/" + itos(i) + "/node"),
    //        PropertyHint::ResourceType, "AnimationRootNode", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL),
    //        "_add_blend_point", "get_blend_point_node", i); ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT,
    //        StringName("blend_point/" + itos(i) + "/pos"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR |
    //        PROPERTY_USAGE_INTERNAL), "set_blend_point_position", "get_blend_point_position", i);
    //    */

    //    Vector<PropertyInterface> nested_indexed;
    //    PropertyInterface bp;
    //    bp.cname = "Blend Points";
    //    bp.max_property_index = 64;
    //    PropertyInterface::TypedEntry te;
    //    te.index=0;
    //    te.entry_type=TypeReference {"Object"};
    //    te.subfield_name = "node";
    //    te.getter="get_blend_point_node";
    //    te.setter="_add_blend_point";
    //    bp.indexed_entries.push_back(te);
    //    te.index=0;
    //    te.entry_type=TypeReference {"float"};
    //    te.subfield_name = "pos";
    //    te.getter="get_blend_point_position";
    //    te.setter="set_blend_point_position";
    //    bp.indexed_entries.push_back(te);

    //    if(bp.max_property_index>0) {
    //        process_array_property(bp,ctx.impl_cs_ctx());
    //    }
    //    bp={};
    //    bp.cname = "Interface";
    //    bp.max_property_index = -2;
    //    te.index=-1;
    //    te.entry_type=TypeReference {"bool"};
    //    te.subfield_name = "is_primary";
    //    te.getter="is_primary";
    //    te.setter="set_is_primary";
    //    bp.indexed_entries.push_back(te);
    //    te.index=1;
    //    te.entry_type=TypeReference {"bool"};
    //    te.subfield_name = "is_initialized";
    //    te.getter="is_initialized";
    //    te.setter="set_is_initialized";
    //    bp.indexed_entries.push_back(te);

    //    if(bp.max_property_index==-2) {
    //        process_group_property(bp,ctx.impl_cs_ctx());
    //    }
}
void register_core_types() {
    StringName::setup();
}

static Vector<String> search_paths;
static Map<String,ReflectionData *> resolved_imports;

static String resolve_import_path(const String &import_name) {
    QString filename((import_name+".json").c_str());
    QFileInfo fi(filename);
    if(fi.exists() && fi.isReadable())
        return qPrintable(filename);
    for(const String &p : search_paths) {
        QString basepath((p+"/").c_str());
        QFileInfo base_fi(basepath+filename);
        if(base_fi.exists() && base_fi.isReadable())
            return qPrintable(basepath+filename);
    }
    return "";
}

static void resolve_imports(ReflectionData &rd) {
    for (auto &import : rd.imports) {
        String import_path(resolve_import_path(import.module_name));
        if(resolved_imports.contains(import_path)) {
            import.resolved = resolved_imports[import_path];
            continue;
        }
        ReflectionData *import_data = new ReflectionData;
        resolved_imports[import_path] = import_data;

        if (import_path.empty() || !import_data->load_from_file(import_path)) {
            qCritical() << "Failed to import required reflection data for module" << import.module_name.c_str();
            exit(-1);
        }
        // Verify the loaded file against the requirement.
        if (import_data->api_version != import.api_version) {
            qCritical() << "Imported reflection data version mismatch" << import_data->api_version.c_str() << " wanted "
                        << import.api_version.c_str();
            exit(-1);
        }
        import.resolved = import_data;
        // Resolve nested imports.
        if(!import_data->imports.empty()) {
            resolve_imports(*import_data);
        }
    }

}
int main(int argc, char **argv) {

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("binding_generator");
    QCoreApplication::setApplicationVersion("0.2");

    QCommandLineParser parser;
    QCommandLineOption ImportOption(QStringList() << "I", "adds an import path to search", "import_path");
    parser.setApplicationDescription("Test helper");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(ImportOption);
    parser.addPositionalArgument("source", "Main reflection json file");
    parser.addPositionalArgument("docs", "documentation directory, scanned recursively for xml doc files");
    parser.addPositionalArgument("target", "destination directory");
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 3) {
        parser.showHelp(-1);
    }

    for(const QString & val : parser.values(ImportOption)) {
        search_paths.emplace_back(qPrintable(val));
    }

    register_core_types();

    ReflectionData rd;
    if (!rd.load_from_file(qPrintable(args[0]))) {
        qCritical() << "Binding generator failed to load source reflection data:" << args[0];
        return -1;
    }
    resolved_imports.emplace(qPrintable(args[0]),&rd);
    resolve_imports(rd);

    DocData docs;
    if (OK != docs.load_classes(qPrintable(args[1]), true)) {
        qCritical("Failed to read documentation files");
    }
    rd.doc = &docs;

    QFileInfo fi(args[2]);
    if ((fi.exists() && !fi.isDir()) || (fi.exists() && fi.isDir() && !fi.isWritable())) {
        qCritical() << "Provided target path is not a writeable directory!" << args[2];
        return -1;
    }

    ProjectContext pr_ctx(rd, args[2]);
    TS_TypeMapper::get().register_default_types();

    //pr_ctx.enter_type("test.cs");
    process(pr_ctx);
    for (auto &import : rd.imports) {
        delete import.resolved;
    }
    return 0;
}
