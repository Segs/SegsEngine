#pragma once

#include "visitor_interface.h"

#include <QString>
#include <QMap>
#include <QVector>
#include <QDebug>

struct TS_TypeLike;
class VisitorInterface;

enum class CSAccessLevel { Public, Internal, Protected, Private };
enum class APIType {
    Invalid = -1,
    Common = 0,
    Editor,
    Client,
    Server,
};

enum class TypeRefKind : int8_t {
    Simple, //
    Enum,
    Array,
};
enum class TypePassBy : int8_t {
    Value = 0, // T
    Reference, // T &
    ConstReference, // const T &
    Move, // T &&
    Pointer,
    ConstPointer,
    MAX_PASS_BY
};

struct TS_Base {
    enum TypeKind {
        NAMESPACE,
        CLASS,
        ENUM,
        FUNCTION,
        PROPERTY,
        SIGNAL,
        CONSTANT,
        FIELD, // it's a name + type_reference;
    };
    const TS_TypeLike *enclosing_type = nullptr;
    QString name;
    TS_Base(const QString &n) : name(n) {}
    virtual TypeKind kind() const = 0;
    virtual QString c_name() const { return name; }
    virtual void accept(VisitorInterface *) const = 0;
    virtual ~TS_Base() {}
};

struct TypeReference {
    TypeRefKind type_kind = TypeRefKind::Simple;
    TypePassBy pass_by = TypePassBy::Value;
    QString name;
    QString template_argument;
    TS_Base *resolved = nullptr;

    TypeReference(QString n, TypeRefKind en = TypeRefKind::Simple, TS_Base *b = nullptr) :
            type_kind(en), name(n), resolved(b) {}
    TypeReference() = default;
};

struct TS_Field : TS_Base {
    TypeReference field_type;
    TypeKind kind() const override { return FIELD; }

    TS_Field(QString n, TypeReference f = {}) : TS_Base(n),field_type(f) {}
    TS_Field() : TS_Base("") {}
    void accept(VisitorInterface *vi) const override { vi->visit(this); }
};

struct TS_Constant : public TS_Base {
    TypeReference const_type{ "int32_t", TypeRefKind::Simple };
    QString value;
    CSAccessLevel access_level = CSAccessLevel::Public;
    bool m_imported = false; //!< if set to true, this constant is an imported one and should not be generated

    static TS_Constant *get_instance_for(const TS_TypeLike *tl);
    QString relative_path(const TS_TypeLike *rel_to = nullptr) const;

    TS_Constant(const QString &p_name, int p_value) : TS_Base(p_name), value(QString::number(p_value)) {}
    TS_Constant(const QString &p_name, const QString &p_value) :
            TS_Base(p_name),const_type({ "String", TypeRefKind::Simple }), value(p_value) {}

    TypeKind kind() const override { return CONSTANT; }
    void accept(VisitorInterface *vi) const override { vi->visit(this); }
};

struct TS_Function : public TS_Base {
    TypeReference return_type;
    QVector<TypeReference> arg_types;
    QVector<QString> arg_values; // name of variable or a value.
    QVector<bool> nullable_ref; // true if given parameter is nullable reference, and we need to always pass a valid pointer.
    QMap<int, QString> arg_defaults;
    bool m_virtual = false;
    bool m_static = false;
    bool m_imported = false; // if true, the methods is imported and should not be processed by generators etc.
    TypeKind kind() const override { return FUNCTION; }
    explicit TS_Function(const QString &n) : TS_Base(n) {}
    void accept(VisitorInterface *vi) const override { vi->visit(this); }
};

struct TS_Signal : public TS_Function {
    explicit TS_Signal(const QString &n) : TS_Function(n) { return_type = { "void", TypeRefKind::Simple }; }
    TypeKind kind() const override { return SIGNAL; }
    void accept(VisitorInterface *vi) const override { vi->visit(this); }
};

struct TS_Property : public TS_Base  {
    struct ResolvedPropertyEntry {
        QString subfield_name;
        QVector<TypeReference> entry_type; // can be more than one type for some resource types.
        QString setter;
        QString getter;
        int index;
    };
    int max_property_index=-1; // -1 for plain properties, -2 for indexed properties, >0 for arrays of multiple properties it's the maximum number.
    QVector<ResolvedPropertyEntry> indexed_entries;
    QStringList usage_flags;
    explicit TS_Property(const QString &n) : TS_Base(n) {  }
    TypeKind kind() const override { return PROPERTY; }
    void accept(VisitorInterface *vi) const override { vi->visit(this); }

};

struct TS_TypeLike : public TS_Base {
public:
    QString required_header;
    explicit TS_TypeLike(const QString &n) : TS_Base(n) {}

    // Nested types - (enum,type) in type, (namespace,enum,type) in namespace, () in enum
    QVector<TS_Base *> m_children;
    bool m_imported = false;
    bool m_skip_special_functions = false; // modules extending imported class should not generate special functions.

    // find a common base type for this and with
    virtual const TS_TypeLike *common_base(const TS_TypeLike *with) const;

    virtual bool enum_name_would_clash_with_property(QString /*cs_enum_name*/) const { return false; }
    virtual bool needs_instance() const { return false; }

    void visit_kind(TypeKind to_visit, std::function<void(const TS_Base *)> visitor) const;
    bool has_named(std::function<bool(TypeKind)> predicate, QStringView name, bool check_enclosing = false) const;
    virtual void add_child(TS_Namespace *) { assert(!"cannot add Namespace to this type"); }
    virtual void add_child(TS_Type *);
    virtual void add_child(TS_Enum *);
    virtual void add_child(TS_Constant *);
    virtual void add_child(TS_Function *) { assert(!"cannot add Function to this type"); }
    virtual void add_child(TS_Property *) { assert(!"cannot add Property to this type"); }
    virtual void add_child(TS_Field *) { assert(!"cannot add Field to this type"); }

    QString relative_path(const TS_TypeLike *rel_to = nullptr) const;

    TS_Constant *add_constant(QString name, QString value);
    void add_enum(TS_Enum *enm);
};

struct TS_Namespace : public TS_TypeLike {
    friend struct TS_Module;

public:
    // static TS_Namespace *get_instance_for(const String &access_path, const NamespaceInterface &src);

    static TS_Namespace *from_path(QStringView path);
    explicit TS_Namespace(const QString &n) : TS_TypeLike(n) {}

    TypeKind kind() const override { return NAMESPACE; }

    void add_child(TS_Namespace *ns) override {
        ns->enclosing_type = this;
        m_children.push_back(ns);
    }
    void accept(VisitorInterface *vi) const override { vi->visit(this); }
    void add_child(TS_Function *m) override {
        m->enclosing_type = this;
        if(!m->m_static) {
            qDebug() << "Marking function '"<<m->name<<"' as static since it was added through a namespace";
            m->m_static = true;
        }
        m_children.push_back(m);
    }

};

struct TS_Enum : public TS_TypeLike {
    QString static_wrapper_class;
    TypeReference underlying_val_type;
    bool is_strict=false; //!< this is a strict enum
    TypeKind kind() const override { return ENUM; }
    QString c_name() const override {
        if (!static_wrapper_class.isEmpty()) { // for synthetic enums - those that don't actually have mapped struct but
                                               // their name refer to it by `StructName::` syntax
            if (name.startsWith(static_wrapper_class))
                return name.mid(static_wrapper_class.size() + 2); // static classname + "::"
        }
        return name;
    }
    explicit TS_Enum(const QString &n) : TS_TypeLike(n) {}
    void add_child(TS_Constant *t) override;
    void accept(VisitorInterface *vi) const override { vi->visit(this); }
};

struct TS_Type : public TS_TypeLike {
    TypeReference base_type;

    mutable int pass = 0;
    bool m_value_type = false; // right now used to mark struct types
    bool is_singleton = false;
    //! Mark type as opaque for binding purposes -> conversion is done purely on the script side.
    bool is_opaque = false;

    static TS_Type *create_type(const TS_TypeLike *owning_type);

    TypeKind kind() const override { return CLASS; }
    // If this object is not a singleton, it needs the instance pointer.
    bool needs_instance() const override { return !is_singleton; }

    QString get_property_path_by_func(const TS_Function *f) const;

    explicit TS_Type(const QString &n) : TS_TypeLike(n) {}
    void accept(VisitorInterface *vi) const override { vi->visit(this); }
    void add_child(TS_Function *m) override {
        m->enclosing_type = this;
        m_children.push_back(m);
    }
    void add_child(TS_Property *m) override {
        m->enclosing_type = this;
        m_children.push_back(m);
    }
    void add_child(TS_Field *m) override {
        m->enclosing_type = this;
        m_children.push_back(m);
    }

};
