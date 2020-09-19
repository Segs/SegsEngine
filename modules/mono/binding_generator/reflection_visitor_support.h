#pragma once
#include "core/vector.h"

struct ProjectContext;
struct ReflectionData;
struct TS_Module;
struct NamespaceInterface;
struct TS_Namespace;
struct TS_Type;
struct TS_Enum;
struct TS_TypeLike;

struct ReflectionVisitorBase {
    explicit ReflectionVisitorBase(ProjectContext &ctx) : m_ctx(ctx) {}
    virtual ~ReflectionVisitorBase() = default;

    String current_access_path() const;
    void leaveNamespace() {
        m_namespace_stack.pop_back();
    }

    virtual void visit(const ReflectionData *refl) = 0; //do_visit_recursive(refl,resolved,&m_ctx.m_rd!=refl);

    virtual void visitModule(const ReflectionData *,bool is_imported=false);
    virtual void visitModule(TS_Module *);
    virtual void finalize() {}


protected:
    Vector<TS_Namespace *> m_namespace_stack;
    Vector<TS_Type *> m_type_stack;
    TS_Module *m_current_module=nullptr;
    ProjectContext &m_ctx;
    TS_Enum *m_current_enum = nullptr;

    void do_visit_recursive(const ReflectionData *refl, bool resolved, bool imported);
    virtual void visitNamespace(const NamespaceInterface &) {}
    virtual void visitNamespace(TS_Namespace *) {}
};

