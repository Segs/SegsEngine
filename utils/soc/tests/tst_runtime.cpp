#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTest>
#include <QBuffer>
#include <QDirIterator>
#include <QJsonArray>

#include "dummy_runtime_classes/a1.h"

class SocRuntimeTest: public QObject
{
    Q_OBJECT
private slots:
    void simple_static_query() {
        Simple s;
        Complex cm;
        SEMetaObject *s_meta=getMetaObject(&s);
        SEMetaObject *cm_meta=getMetaObject(&cm);
        QVERIFY(s_meta!=nullptr);
        QVERIFY(cm_meta!=nullptr);
        QVERIFY(s_meta!=cm_meta);
        QCOMPARE(s_meta->methodCount(),0);
        QCOMPARE(cm_meta->methodCount(),1);
    }
};


QTEST_MAIN(SocRuntimeTest)
#include "tst_runtime.moc"
