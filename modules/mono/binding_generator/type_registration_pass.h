#pragma once

#include "reflection_visitor_support.h"

struct TS_Constant;
struct TS_Function;
struct TS_TypeLike;
struct TS_Enum;
struct TS_Type;
struct TS_Property;

struct ConstantInterface;
struct MethodInterface;
struct EnumInterface;
struct TypeInterface;
struct PropertyInterface;

struct TypeRegistrationPass : public ReflectionVisitorBase {

    bool m_currently_visiting_imported=false;
    TS_Module *m_current_module=nullptr;

    static bool covariantSetterGetterTypes(StringView getter, StringView setter);
    static int _determine_enum_prefix(const TS_Enum &p_ienum);
    static void _apply_prefix_to_enum_constants(const TS_Enum &p_ienum, int p_prefix_length);

    explicit TypeRegistrationPass(ProjectContext &ctx) : ReflectionVisitorBase(ctx) {}
    void visitConstant(const ConstantInterface *ci);
    void visitEnum(const EnumInterface *ei);
    void visitMethodInterface(const MethodInterface *fi);
    void visitTypeProperty(const PropertyInterface *pi);

    void registerTypesPass(const TypeInterface *ti);
    void registerTypeDetails(const TS_Type *type);
    void visitModule(const ReflectionData *rd,bool imported=false) override;
    void visitNamespace(const NamespaceInterface &iface) override;
    void finalize() override;
    void visit(const ReflectionData *refl) override;

};
