#include "reflection_walker.h"

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTest>
#include <QBuffer>
#include <QDirIterator>
#include <QJsonArray>

class SocTest: public QObject
{
    Q_OBJECT

private:
    bool isValidJson(const QByteArray &dat) {
        QJsonParseError err;
        QJsonDocument::fromJson(dat,&err);
        if(err.error!=QJsonParseError::NoError)
            qCritical() << err.errorString();
        return err.error==QJsonParseError::NoError;
    }
    void compareJson(const QJsonArray &a,const QJsonArray &b) {
        int max_idx = std::min(a.size(),b.size());
        for(int i=0; i<max_idx; ++i) {
            QJsonValue v1 = a[i];
            QJsonValue v2 = b[i];
            QCOMPARE(v1.type(),v2.type());
            if(v1.isObject()) {
                compareJson(v1.toObject(),v2.toObject());
            } else if(v1.isArray()) {
                compareJson(v1.toArray(),v2.toArray());
            } else {
                QCOMPARE(v1,v2);
            }
        }
    }
    void compareJson(const QJsonObject &a,const QJsonObject &b) {
        auto a_keys = a.keys();
        auto b_keys = b.keys();
        int max_idx = std::min(a_keys.size(),b_keys.size());
        for(int i=0; i<max_idx; ++i) {
            QCOMPARE(a_keys[i],b_keys[i]);
            QJsonValue v1 = a[a_keys[i]];
            QJsonValue v2 = b[a_keys[i]];
            QCOMPARE(v1.type(),v2.type());
            if(v1.isObject()) {
                compareJson(v1.toObject(),v2.toObject());
            } else if(v1.isArray()) {
                compareJson(v1.toArray(),v2.toArray());
            } else {
                QCOMPARE(v1,v2);
            }
        }
        QCOMPARE(a_keys.size(),b_keys.size());
    }
private slots:
    void allTests_data() {
        QTest::addColumn<QByteArray>("source");
        QTest::addColumn<QByteArray>("expected");
        QDirIterator test_cases("test_cases",{ "*.h" }, QDir::NoFilter, QDirIterator::Subdirectories);
        while(test_cases.hasNext()) {
            QFileInfo fi(test_cases.next());
            QFile src_h(fi.filePath());
            QFile src_json(QString(fi.filePath()).replace(".h",".json"));
            if(!src_h.open(QFile::ReadOnly)) {
                qCritical() << "Failed to open test case header"<<fi.filePath();
            }
            if(!src_json.open(QFile::ReadOnly)) {
                qCritical() << "Failed to open test case target"<<src_json.fileName();
            }
            QByteArray src_json_content=src_json.readAll();
            QTest::newRow(qPrintable(fi.fileName())) << QByteArray(src_h.readAll()) << src_json_content;
        }
    }

    void allTests()
    {
        QFETCH(QByteArray, source);
        QFETCH(QByteArray, expected);

        initContext();
        ModuleConfig config;
        config.default_ns = "GodotCore";
        setConfig(config);

        QByteArray result;
        QBuffer buf(&source);
        QBuffer res(&result);

        res.open(QIODevice::WriteOnly);
        buf.open(QIODevice::ReadOnly);
        // Check for expected failures
        bool processing = processHeader(QTest::currentDataTag(),&buf);
        if(expected.size()==0) {
            QCOMPARE(processing,false);
            return;
        }
        QVERIFY(isValidJson(expected));

        exportJson(&res);

        QVERIFY(!result.isEmpty());
        QVERIFY(isValidJson(result));
        QString result_min(result);
        QString expected_min(expected);
        expected_min.remove(' ');
        result_min.remove(' ');
        compareJson(QJsonDocument::fromJson(result_min.toLatin1()).object(),QJsonDocument::fromJson(expected_min.toLatin1()).object());
    }

};

QTEST_MAIN(SocTest)
#include "tst_namespace.moc"
