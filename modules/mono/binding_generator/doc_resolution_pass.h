#pragma once

#include "reflection_visitor_support.h"

struct TS_Constant;
struct TS_Function;
struct TS_Signal;
struct TS_TypeLike;
struct TS_Enum;
struct TS_Type;
struct TS_Property;

struct DocResolutionPass : public ReflectionVisitorBase {
    DocResolutionPass(ProjectContext &ctx) : ReflectionVisitorBase(ctx) {}

    void resolveTypeDocs(TS_TypeLike *tgt) const;
    static void _resolveFuncDocs(TS_Function *tgt);
    static void _resolveFuncDocs(TS_Signal *tgt);

    void visitConstant(TS_Constant *ci);
    void visitEnum(TS_Enum *en);
    static void visitFunction(TS_Function *func);
    static void visitSignal(TS_Signal *func);
    static void visitTypeProperty(TS_Property *prop);
    void visitType(TS_Type *type);
    void visitNamespace(TS_Namespace *ns) override;
    void visit(const ReflectionData *rd) override { do_visit_recursive(rd,true,false); }
};
