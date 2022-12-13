#include "doc_resolution_pass.h"

#include "type_system.h"
#include "type_mapper.h"
#include "generator_helpers.h"

#include <QDebug>

void DocResolutionPass::resolveTypeDocs(TS_TypeLike *tgt) const {
    StringView type_name(tgt->c_name());
    // types starting with '_' are assumed to wrap the non-prefixed class for script acces.
    if (type_name.starts_with('_')) 
        type_name = type_name.substr(1);
    auto res = m_ctx.m_rd.doc->class_list.find_as(type_name);
    if (m_ctx.m_rd.doc->class_list.end() == res)
        res = m_ctx.m_rd.doc->class_list.find(tgt->relative_path(CPP_IMPL));
    if (m_ctx.m_rd.doc->class_list.end() != res) {
        tgt->m_docs = &res->second;
        return;
    }
    if(tgt->kind()!=TS_TypeLike::ENUM) {
        qDebug() << "Failed to find docs for" << tgt->relative_path(CPP_IMPL).c_str();
    }
}

void DocResolutionPass::_resolveFuncDocs(TS_Function *tgt) {
    if (!tgt->enclosing_type || !tgt->enclosing_type->m_docs) return;
    const TS_TypeLike *iter = tgt->enclosing_type;
    const DocContents::MethodDoc *located_docs = nullptr;
    while (iter) {
        const DocContents::ClassDoc *our_class_docs = iter->m_docs;
        auto doc_data = our_class_docs ? our_class_docs->func_by_name(tgt->source_type->name) : nullptr;
        if (doc_data) {
            located_docs = doc_data;
            break;
        }
        // try in base class.
        if (iter->kind() == TS_TypeLike::CLASS) {
            iter = ((const TS_Type *)iter)->base_type;
        } else
            break;
    }

    tgt->m_resolved_doc = located_docs;
}
void DocResolutionPass::_resolveFuncDocs(TS_Signal *tgt) {
    if (!tgt->enclosing_type || !tgt->enclosing_type->m_docs) return;
    const TS_TypeLike *iter = tgt->enclosing_type;
    const DocContents::MethodDoc *located_docs = nullptr;
    while (iter) {
        const DocContents::ClassDoc *our_class_docs = iter->m_docs;
        auto doc_data = our_class_docs ? our_class_docs->signal_by_name(tgt->source_type->name) : nullptr;
        if (doc_data) {
            located_docs = doc_data;
            break;
        }
        // try in base class.
        if (iter->kind() == TS_TypeLike::CLASS) {
            iter = ((const TS_Type *)iter)->base_type;
        } else
            break;
    }

    tgt->m_resolved_doc = located_docs;
}
void DocResolutionPass::visitConstant(TS_Constant *ci) {
    TS_TypeLike *enclosing;
    if (m_current_enum) {
        enclosing = m_current_enum;

    } else if (m_type_stack.empty()) {
        assert(!m_namespace_stack.empty());
        enclosing = m_namespace_stack.back();
    } else {
        enclosing = m_type_stack.back();
    }
    if (enclosing->m_docs) {
        if (enclosing->kind() != TS_TypeLike::ENUM)
            ci->m_resolved_doc = enclosing->m_docs->const_by_name(ci->c_name().c_str());
        else
            ci->m_resolved_doc = enclosing->m_docs->const_by_enum_name(ci->c_name().c_str());
    }

    auto docs = enclosing->m_docs;
    if(docs)
        ci->m_resolved_doc = docs->const_by_name(ci->m_rd_data->name.c_str());
}

void DocResolutionPass::visitEnum(TS_Enum *en) {
    resolveTypeDocs(en);
    // right now there are no direct docs for enums, they use their enclosing class constant docs instead.
    if (en->nested_in->m_docs)
        en->m_docs = en->nested_in->m_docs;
    else {
        // try to find some docs in parent type?
        const TS_TypeLike *iter = en->nested_in;
        while (iter) {
            en->m_docs = iter->m_docs;
            if (en->m_docs) break;
            iter = iter->nested_in;
        }
    }

    m_current_enum = en;
    for (auto ci : en->m_constants) {
        visitConstant(ci);
    }
    m_current_enum = nullptr;
}

void DocResolutionPass::visitFunction(TS_Function *func) {
    _resolveFuncDocs(func);

    if (func->m_resolved_doc) {
        int idx = 0;
        // Replace generic names with those from documentation.
        for (const auto &doc : func->m_resolved_doc->arguments) {
            if (!doc.name.empty() && func->arg_values[idx].starts_with("arg")) {
                func->arg_values[idx] = escape_csharp_keyword(doc.name);
            }
            ++idx;
        }
    }
}
void DocResolutionPass::visitSignal(TS_Signal *func) {
    _resolveFuncDocs(func);

    if (!func->m_resolved_doc) {
        return;
    }
    int idx = 0;
    // Replace generic names with those from documentation.
    for (const auto &doc : func->m_resolved_doc->arguments) {
        if (!doc.name.empty() && func->arg_values[idx].starts_with("arg")) {
            func->arg_values[idx] = escape_csharp_keyword(doc.name);
        }
        ++idx;
    }
}
void DocResolutionPass::visitTypeProperty(TS_Property *prop) {
    auto owner_type = prop->m_owner;
    if (owner_type && owner_type->m_docs) {
        for(auto &e : prop->indexed_entries) {
            StringView search_by = e.setter ? e.setter->c_name() : e.getter->c_name();
            e.m_docs = owner_type->m_docs->property_by_func_name(search_by);

        }
    }
}

void DocResolutionPass::visitType(TS_Type *type) {
    assert(type && type->pass > 0);

    m_type_stack.push_back(type);

    resolveTypeDocs(type);

    for (TS_Constant *ci : type->m_constants) {
        visitConstant(ci);
    }
    for (TS_TypeLike *ci : type->m_children) {
        switch (ci->kind()) {
        case TS_TypeLike::NAMESPACE: visitNamespace((TS_Namespace *)ci); break;
        case TS_TypeLike::CLASS: visitType((TS_Type *)ci); break;
        case TS_TypeLike::ENUM: visitEnum((TS_Enum *)ci); break;
        }
    }

    // Properties use class methods for setters/getters, so we visit methods first.
    for (TS_Function *mi : type->m_functions) {
        visitFunction(mi);
    }

    for (TS_Property *pi : type->m_properties) {
        visitTypeProperty(pi);
    }

    for (TS_Signal *pi : type->m_signals) {
        visitSignal(pi);
    }
    m_type_stack.pop_back();
}

void DocResolutionPass::visitNamespace(TS_Namespace *ns) {
    m_namespace_stack.push_back(ns);
    m_namespace_stack.back()->m_docs = &m_ctx.m_rd.doc->class_doc("@GlobalScope");

    // TODO: handle namespace docs stuff in the future
    for (TS_Constant *ci : ns->m_constants) {
        visitConstant(ci);
    }
    for (TS_TypeLike *ci : ns->m_children) {
        switch (ci->kind()) {
        case TS_TypeLike::NAMESPACE: visitNamespace((TS_Namespace *)ci); break;
        case TS_TypeLike::CLASS: visitType((TS_Type *)ci); break;
        case TS_TypeLike::ENUM: visitEnum((TS_Enum *)ci); break;
        }
    }
    leaveNamespace();
}
