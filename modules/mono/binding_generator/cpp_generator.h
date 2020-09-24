#pragma once
#include "reflection_visitor_support.h"
#include "generator_helpers.h"
#include "core/hash_set.h"

#include "EASTL/vector_map.h"

struct CppGeneratorVisitor: public ReflectionVisitorBase  {
    using Super = ReflectionVisitorBase;
    // Each top level namespace generates a single cpp file.
    Vector<GeneratorContext> m_namespace_files;
    Vector<String> m_registration_names;
    GeneratorContext m_plugin_header;
    GeneratorContext m_plugin_wrapper;

    HashSet<String> m_known_includes;
    String plugin_name= "GodotCore";
    CppGeneratorVisitor(ProjectContext &ctx);

    void finalize() override;
    void visitModule(TS_Module * mod) override;
    void visitNamespace(TS_Namespace * ns) override;
    void visit(const ReflectionData *refl) override;
protected:
    void visitType_CollectHeaders(ProjectContext &ctx,const TS_TypeLike * iface);
    void visitType(ProjectContext &ctx,const TS_TypeLike * iface);
    void mapFunctionArguments(const TS_Function *finfo);
    void verifyMethodSelfPtr(const TS_Function *finfo,bool non_void_return);
    String prepareInstanceVariable(const TS_Function *finfo);

    void prepareArgumentLocals(const TS_Function *finfo, eastl::vector_map<String, String> &mapped_args);
    String mapReturnType(const TS_Function *finfo);
    void visitFunction(const TS_Function *finfo);
    void fillPluginMetadata(QJsonObject &tgt);

    Vector<String> m_path_components;
};

