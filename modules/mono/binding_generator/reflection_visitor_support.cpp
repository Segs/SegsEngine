#include "reflection_visitor_support.h"

#include "type_system.h"

#include "core/string.h"
#include "core/string_builder.h"

String ReflectionVisitorBase::current_access_path() const {
    StringBuilder res;
    for(const TS_Namespace *ns : m_namespace_stack) {
        res += ns->cs_name();
        res += "::";
    }
    for(const TS_Type *ts : m_type_stack) {
        res += ts->cs_name();
        res += "::";
    }
    if(m_current_enum) {
        res += m_current_enum->cs_name();
        res += "::";
    }
    return res;
}

void ReflectionVisitorBase::visitModule(const ReflectionData *refl, bool is_imported) {
    for (const NamespaceInterface& iface : refl->namespaces) {
        visitNamespace(iface);
    }
}

void ReflectionVisitorBase::visitModule(TS_Module *mod) {
    m_current_module = mod;
    for(const auto &v : mod->m_namespaces) {
        visitNamespace(v.second);
    }
    m_current_module = nullptr;
}

void ReflectionVisitorBase::do_visit_recursive(const ReflectionData *refl,bool resolved,bool imported) {
    for(auto & imp : refl->imports) {
        do_visit_recursive(imp.resolved,resolved,true);
    }

    if(!resolved) {
        visitModule(refl,imported);
    }
    else {
        //NOTE: if this fails, the module was not registered by TypeRegistrationPass
        assert(TS_Module::find_module(refl->module_name));
        visitModule(TS_Module::find_module(refl->module_name));
    }

}
