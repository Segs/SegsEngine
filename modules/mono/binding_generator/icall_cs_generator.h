#pragma once

#include "generator_helpers.h"
#include "reflection_visitor_support.h"
#include "core/hash_set.h"

struct TS_Function;

struct CsInterfaceVisitor : public ReflectionVisitorBase
{
    using Super = ReflectionVisitorBase;
    // Each top level namespace generates a single interface file.
    Vector<GeneratorContext> m_namespace_files;
    HashSet<String> m_known_imports;

public:
    CsInterfaceVisitor(ProjectContext& ctx);
    void visit(const ReflectionData *refl) override;
    void visitModule(TS_Module *) override;
    void finalize() override;
protected:
    void visitType(const ProjectContext &ctx, TS_TypeLike *ns);
    void visitNamespace(TS_Namespace *) override;

    void visitFunction(const TS_Function* finfo);
    void mapFunctionArguments(const TS_Function *finfo);
    String mapReturnType(const TS_Function *finfo);
    Vector<String> m_path_components;
};

