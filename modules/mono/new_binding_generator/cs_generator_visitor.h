#pragma once

#include "generator_helpers.h"
#include "reflection_visitor_support.h"
#include <EASTL/vector_map.h>

struct CsGeneratorVisitor : public ReflectionVisitorBase {
    explicit CsGeneratorVisitor(ProjectContext &ctx) : ReflectionVisitorBase(ctx) {

    }
    ~CsGeneratorVisitor() override;
    void visit(const ReflectionData *refl) override {
        do_visit_recursive(refl,true,false);
    }
    void finalize() override;
    void visitModule(TS_Module *) override;

protected:
    void visitNamespace(TS_Namespace *iface) override;
    void visitType(TS_TypeLike *tp);
    void visitNSInternal(TS_Namespace *tp);
    void visitClassInternal(TS_Type *tp);
    void generateSpecialFunctions(TS_TypeLike *itype, GeneratorContext &ctx);

    Vector<GeneratorContext *> m_gen_files;
    Vector<GeneratorContext *> m_gen_stack;
    Vector<String> m_path_components;
};

String gen_func_args(const TS_Function &finfo,const eastl::vector_map<String, String> &mapped_args);
