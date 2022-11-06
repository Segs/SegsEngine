#include "cpp_visitor.h"

#include "type_system.h"
#include <QSet>
namespace  {
/*
#define SELF_VIEW(var,offset,count) {(qt_meta_stringdata_ ## var).stringdata+offset,count}
struct qt_meta_stringdata_SocTest_t {
    StringView data[4];
    char stringdata[32];
} qt_meta_stringdata_SocTest = {
    { SELF_VIEW(SocTest,0,7),

}
    ,
        "SocTest\0allTests_data\0\0allTests"
};

static const uint qt_meta_data_SocTest[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   24,    2, 0x08 / * Private * /,
       3,    0,   25,    2, 0x08 / * Private * /,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

QT_INIT_METAOBJECT const SEMetaObject Complex_staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_SocTest.data,
    qt_meta_data_SocTest,
    qt_static_metacall,
    nullptr,
    nullptr
} };

*/

QString typeToVariantType(const TS_Property::ResolvedPropertyEntry &rt) {
    assert(rt.entry_type.size()==1);
    const auto &tp(rt.entry_type.front());
    if(tp.pass_by==TypePassBy::Value) {
        if(tp.name.startsWith("int") && tp.name.endsWith("_t")) {
            return "Int";
        }
        if(tp.name=="RefPtr") {
            return "OBJECT";
        }
    }
    return "";
}
QString buildPropertyInfo(const TS_Property *from) {
    QString res = R"(PropertyInfo(VariantType::%1, "%2", %3, "%4", %5))";
    if(from->max_property_index==-1) {
        const TS_Property::ResolvedPropertyEntry &rt(from->indexed_entries.front());
        return res.arg(typeToVariantType(rt)).arg(from->name);
    }
    assert(false);
    return "";
}
struct CppVisitor : public VisitorInterface {
    QSet<QString> headers;
    QMap<QString, QStringList> class_reflection_support;
    QMap<QString, QStringList> class_binders;
    QVector<QString> class_stack; // for nested classes.
    // VisitorInterface interface
    void visit(const TS_Enum *entry) override {

    }
    void visit(const TS_Type *entry) override {
        // Generate meta object lookup instantiation

        QString name = entry->c_name();
        QString mo_name = name+"_staticMetaObject";
        class_stack.push_back(name);
        assert(class_binders.contains(name) == false);
        assert(class_reflection_support.contains(name) == false);
        QStringList & cb(class_reflection_support[name]);

        cb.append(QString("static SEMetaObject %1;").arg(mo_name));
        cb.append(QString(R"(template<>
SEMetaObject *getMetaObject<%1>(%1 *self) {
    if constexpr(eastl::is_base_of<IReflectable,%1>()) {
        auto refl=(IReflectable *)(self);
        return refl->hasDynamicMetaObject() ? refl->dynamicMetaObject() : &%2;
    } else {
        return &%2;
    }
}
)").arg(name,mo_name));
        for (const auto *child : entry->m_children) {
            child->accept(this);
        }
        headers.insert(entry->required_header);
        class_stack.pop_back();
    }
    void visit(const TS_Namespace *entry) override {
        for (const auto *child : entry->m_children) {
            child->accept(this);
        }
    }
    void visit(const TS_Property *entry) override {
        QStringList &contents = class_binders[class_stack.back()];
        contents += QString("ADD_PROPERTY(%1)").arg(buildPropertyInfo(entry));
    }
    void visit(const TS_Signal *entry) override {}
    void visit(const TS_Function *entry) override {
        QStringList &contents = class_binders[class_stack.back()];
        contents += QString("MethodBinder::bind_method(D_METHOD(\"%1\")\n").arg(entry->name);
    }
    void visit(const TS_Constant *entry) override {
        QStringList &contents = class_binders[class_stack.back()];
        contents += QString("BIND_CONSTANT(%1);").arg(entry->name);
    }
    void visit(const TS_Field *entry) override {}
    ~CppVisitor() override = default;
};

}

VisitorInterface *createCppVisitor() {
    return new CppVisitor;
}

void produceCppOutput(VisitorInterface *iface,QIODevice *tgt) {
    auto *cpp_iface=dynamic_cast<CppVisitor *>(iface);
    assert(cpp_iface);
    QString content;
    // TODO: extract all headers from associated cpp file
    QStringList parts = cpp_iface->headers.values();
    parts.sort();
    for(QString &inc : parts) {
        inc = QString("#include \"%1\"").arg(inc);
    }
    content += "#include \"core/reflection_support/reflection_data.h\"\n";
    content += parts.join("\n");
    tgt->write(content.toUtf8());
    tgt->write("\n\n");
    for(const auto & entry : cpp_iface->class_reflection_support) {
        tgt->write(entry.join("\n").toUtf8());
    }
//    for(auto iter= cpp_iface->class_binders.begin(), fin = cpp_iface->class_binders.end(); iter!=fin; ++iter) {
//        QString bind = QString("void ") + iter.key() + "::_init_reflection() {\n";
//        bind += "    " + iter.value().join("\n    ");
//        bind += "}\n";
//        tgt->write(bind.toUtf8());
//    }
}
