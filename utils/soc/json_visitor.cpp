#include "json_visitor.h"

#include "type_system.h"

#include <QList>
#include <QJsonObject>
#include <QJsonArray>
namespace {

template <class T> T valFromJson(const QJsonValue &v);

template <class T> void setJsonIfNonDefault(QJsonObject &obj, const char *field, const T &v) {
    if (v != T()) {
        obj[field] = v;
    }
}

template <class T> void toJson(QJsonObject &tgt, const char *name, const QVector<T> &src) {
    if (src.isEmpty())
        return;
    QJsonArray entries;
    for (const T &c : src) {
        QJsonObject field;
        c.toJson(field);
        entries.push_back(field);
    }
    tgt[name] = entries;
}
template <class T> void fromJson(const QJsonObject &src, const char *name, QVector<T> &tgt) {
    if (!src.contains(name)) {
        tgt.clear();
        return;
    }
    assert(src[name].isArray());
    QJsonArray arr = src[name].toArray();
    tgt.reserve(arr.size());
    for (int i = 0; i < arr.size(); ++i) {
        T ci;
        ci.fromJson(arr[i].toObject());
        tgt.emplace_back(ci);
    }
}

struct JSonVisitor : public VisitorInterface {
    QList<QJsonObject> result;
    // VisitorInterface interface
    void entryToJSON(const TS_TypeLike *tl, TS_Base::TypeKind kind, QJsonObject &tgt) {
        QString entry_name;
        switch (kind) {
            case TS_Base::NAMESPACE:
                entry_name = "namespaces";
                break;
            case TS_Base::CLASS:
                entry_name = "subtypes";
                break;
            case TS_Base::ENUM:
                entry_name = "enums";
                break;
            case TS_Base::FUNCTION:
                entry_name = "functions";
                break;
            case TS_Base::CONSTANT:
                entry_name = "constants";
                break;
            case TS_Base::SIGNAL:
                entry_name = "signals";
                break;
            case TS_Base::PROPERTY:
                entry_name = "properties";
                break;
            case TS_Base::FIELD:
                entry_name = "fields";
                break;
        }
        QJsonArray arr;
        tl->visit_kind(kind, [&](const TS_Base *e) {
            result.push_back(QJsonObject());
            e->accept(this);
            arr.push_back(result.takeLast());
        });
        if (!arr.empty()) {
            tgt[entry_name] = arr;
        }
    }
    void commonVisit(const TS_Base *self) {
        QJsonObject &current(result.back());
        current["name"] = self->name;
    }

    void commonVisit(const TS_TypeLike *self) {
        commonVisit((const TS_Base *)self);
        QJsonObject &current(result.back());
        if (!self->required_header.isEmpty())
            current["required_header"] = self->required_header;
    }
public:
    JSonVisitor() {
        // add first root
        result.push_back(QJsonObject());
    }
    void visit(const TS_Enum *vs) override {
        commonVisit(vs);
        QJsonObject &current(result.back());

        entryToJSON(vs, TS_Base::CONSTANT, current);

        if (vs->underlying_val_type.name != "int32_t" && vs->underlying_val_type.name != "int") {
            current["underlying_type"] = vs->underlying_val_type.name;
        }
        if(vs->is_strict) {
            current["is_strict"] = true;
        }
    }
    void visit(const TS_Type *vs) override {
        commonVisit(vs);
        QJsonObject &current(result.back());
        QJsonObject root_obj;

        entryToJSON(vs, TS_Base::ENUM, root_obj);
        entryToJSON(vs, TS_Base::CONSTANT, root_obj);
        entryToJSON(vs, TS_Base::CLASS, root_obj);
        entryToJSON(vs, TS_Base::FUNCTION, root_obj);
        entryToJSON(vs, TS_Base::PROPERTY, root_obj);
        entryToJSON(vs, TS_Base::SIGNAL, root_obj);
        entryToJSON(vs, TS_Base::FIELD, root_obj);

        current["contents"] = root_obj;
        if(!vs->base_type.name.isEmpty()) {
            current["base_type"] = serializeTypeRef(&vs->base_type);
        }
        setJsonIfNonDefault(current, "is_singleton", vs->is_singleton);
        setJsonIfNonDefault(current, "is_opaque", vs->is_opaque);

    }
    void visit(const TS_Namespace *vs) override {
        commonVisit(vs);
        QJsonObject &current(result.back());
        QJsonObject root_obj;

        entryToJSON(vs, TS_Base::ENUM, root_obj);
        entryToJSON(vs, TS_Base::CONSTANT, root_obj);
        entryToJSON(vs, TS_Base::CLASS, root_obj);
        entryToJSON(vs, TS_Base::FUNCTION, root_obj);
        entryToJSON(vs, TS_Base::NAMESPACE, root_obj);

        current["contents"] = root_obj;
    }
    void visit(const TS_Property *ps) override {
        commonVisit(ps);

        QJsonObject &current(result.back());

        QJsonArray subfields;
        if(ps->max_property_index!=-1) {
            current["max_property_index"]=ps->max_property_index;
        }
        for(const auto & vv : ps->indexed_entries) {
            QJsonObject entry;
            entry["getter"]=vv.getter;
            if(!vv.setter.isEmpty())
                entry["setter"]=vv.setter;
            const auto &e(vv.entry_type.front());
            entry["type"] = serializeTypeRef(&e);
            if(!vv.subfield_name.isEmpty()){
                entry["name"] = vv.subfield_name;
            }
            subfields.push_back(entry);
        }
        current["subfields"] = subfields;
        if(!ps->usage_flags.empty())
            current["usage"] = ps->usage_flags.join('|');

    }
    void visit(const TS_Signal *fs) override {
        commonVisit(fs);

        QJsonObject &current(result.back());

        if(fs->arg_types.empty())
            return;
        QJsonArray array;
        for(int idx=0; idx<fs->arg_types.size(); ++idx) {
            QJsonObject arg_def;

            arg_def["type"]=serializeTypeRef(&fs->arg_types[idx]);
            arg_def["name"]=fs->arg_values[idx];
            auto iter=fs->arg_defaults.find(idx);
            if(iter!=fs->arg_defaults.end()) {
                arg_def["default_argument"] = *iter;
            }
            array.append(arg_def);
        }
        current["arguments"] = array;

    }
    void visit(const TS_Function *fs) override {
        commonVisit(fs);

        QJsonObject &current(result.back());

        QJsonObject return_type;
        current["return_type"] = serializeTypeRef(&fs->return_type);

        if(fs->m_virtual) {
            current["is_virtual"] = fs->m_virtual;
        }
        if(fs->m_static) {
            current["is_static"] = fs->m_static;
        }

        if(fs->arg_types.empty())
            return;

        QJsonArray array;
        for(int idx=0; idx<fs->arg_types.size(); ++idx) {
            QJsonObject arg_def;
            arg_def["type"]=serializeTypeRef(&fs->arg_types[idx]);
            arg_def["name"]=fs->arg_values[idx];
            auto iter=fs->arg_defaults.find(idx);
            if(iter!=fs->arg_defaults.end()) {
                arg_def["default_argument"] = *iter;
            }
            array.append(arg_def);
        }
        current["arguments"] = array;
    }
    void visit(const TS_Constant *cn) override {
        QJsonObject &current(result.back());
        current["name"] = cn->name;
        current["value"] = cn->value;
        if (cn->enclosing_type->kind() != TS_Base::ENUM) {
            // only write out non-redundant info
            current["type"] = serializeTypeRef(&cn->const_type);
        }
    }
    void visit(const TS_Field *f) override {
        QJsonObject &current(result.back());
        current["name"] = f->name;
        current["type"] = serializeTypeRef(&f->field_type);
    }
    QJsonObject serializeTypeRef(const TypeReference *tr) {
        QJsonObject res;
        res["name"] = tr->name;
        setJsonIfNonDefault(res, "template_arg", tr->template_argument);
        setJsonIfNonDefault(res, "is_enum", (int8_t)tr->type_kind);
        if (tr->pass_by != TypePassBy::Value)
            res["pass_by"] = (int8_t)tr->pass_by;
        return res;
    }

};
}

VisitorInterface *createJsonVisitor()
{
    return new JSonVisitor;
}

QJsonObject takeRootFromJsonVisitor(VisitorInterface *iface)
{
    auto *json_iface=dynamic_cast<JSonVisitor *>(iface);
    assert(json_iface);
    assert(json_iface->result.size() == 1);

    return json_iface->result.takeLast();
}
