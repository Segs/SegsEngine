#include "type_system.h"

//!
//! \brief Returns the type access path relative to \a rel_to,
//! if rel_to is nullptr this will return full access path
//! \param tgt
//! \param rel_to
//! \return the string representing the type path
//!
QString TS_TypeLike::relative_path(const TS_TypeLike *rel_to) const {
    QStringList parts;
    QSet<const TS_TypeLike *> rel_path;
    const TS_TypeLike *rel_iter = rel_to;
    while (rel_iter) {
        rel_path.insert(rel_iter);
        rel_iter = rel_iter->enclosing_type;
    }

    const TS_TypeLike *ns_iter = this;
    while (ns_iter && !rel_path.contains(ns_iter)) {
        parts.push_front(ns_iter->name);
        // FIXME: this is a hack to handle Variant.Operator correctly.
        if (kind() == ENUM && ns_iter->name == "Variant" && parts[0] != "Variant") {
            parts[0] = "Variant";
        }
        ns_iter = ns_iter->enclosing_type;
    }
    return parts.join("::");
}

void TS_TypeLike::add_enum(TS_Enum *enm) {
    // TODO: add sanity checks here
    m_children.push_back(enm);
}


const TS_TypeLike *TS_TypeLike::common_base(const TS_TypeLike *with) const {
    const TS_TypeLike *lh = this;
    const TS_TypeLike *rh = with;
    if (!lh || !rh)
        return nullptr;

    // NOTE: this assumes that no type path will be longer than 16, should be enough though ?
    QVector<const TS_TypeLike *> lh_path;
    QVector<const TS_TypeLike *> rh_path;

    // collect paths to root for both types

    while (lh->enclosing_type) {
        lh_path.push_back(lh);
        lh = lh->enclosing_type;
    }
    while (rh->enclosing_type) {
        rh_path.push_back(rh);
        rh = rh->enclosing_type;
    }
    if (lh != rh)
        return nullptr; // no common base

    auto rb_lh = lh_path.rbegin();
    auto rb_rh = rh_path.rbegin();

    // walk backwards on both paths
    while (rb_lh != lh_path.rend() && rb_rh != rh_path.rend()) {
        if (*rb_lh != *rb_rh) {
            // encountered non-common type, take a step back and return
            --rb_lh;
            return *rb_lh;
        }
        ++rb_lh;
        ++rb_rh;
    }
    return nullptr;
}

void TS_TypeLike::visit_kind(TS_Base::TypeKind to_visit, std::function<void (const TS_Base *)> visitor) const {
    for (const TS_Base *tl : m_children) {
        if (tl->kind() == to_visit) {
            visitor(tl);
        }
    }
}

bool TS_TypeLike::has_named(std::function<bool (TS_Base::TypeKind)> predicate, QStringView name, bool check_enclosing) const {
    for (const TS_Base *tl : m_children) {
        if (predicate(tl->kind())) {
            if(name==tl->c_name())
                return true;
        }
    }
    if(check_enclosing && enclosing_type) {
        return enclosing_type->has_named(predicate,name,check_enclosing);
    }
    return false;
}


void TS_TypeLike::add_child(TS_Type *t) {
    t->enclosing_type = this;
    m_children.push_back(t);
}

void TS_TypeLike::add_child(TS_Enum *t) {
    t->enclosing_type = this;
    m_children.push_back(t);
}

void TS_TypeLike::add_child(TS_Constant *t) {
    t->enclosing_type = this;
    m_children.push_back(t);
}

void TS_Enum::add_child(TS_Constant *t) {
    t->enclosing_type = this;
    t->const_type = underlying_val_type;
    m_children.push_back(t);
}
