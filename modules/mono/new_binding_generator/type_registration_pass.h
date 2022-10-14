#pragma once

#include "reflection_visitor_support.h"

struct ConstantInterface;
struct MethodInterface;
struct EnumInterface;
struct TypeInterface;
struct PropertyInterface;
struct SignalInterface;

struct TypeRegistrationPass : public ReflectionVisitorBase {

    bool m_currently_visiting_imported=false;

    static bool covariantSetterGetterTypes(StringView getter, StringView setter);

    explicit TypeRegistrationPass(ProjectContext &ctx) : ReflectionVisitorBase(ctx) {}
    void visitConstant(const ConstantInterface *ci);
    void visitEnum(const EnumInterface *ei);
    void visitMethodInterface(const MethodInterface *fi);
    void visitSignalInterface(const SignalInterface *fi);
    void visitTypeProperty(const PropertyInterface *pi);

    void registerTypesPass(const TypeInterface *ti);
    void registerTypeDetails(const TS_Type *type);
    void visitModule(const ReflectionData *rd,bool imported=false) override;
    void visitNamespace(const NamespaceInterface &iface) override;
    void finalize() override;
    void visit(const ReflectionData *refl) override;

};
