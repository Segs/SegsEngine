#pragma once

#include <QString>
#include <QVector>

class QIODevice;

struct ModuleConfig {
    QString module_name;
    //! default namespace used when one is needed and was not available - a crutch to reduce amount of SE_NAMESPACE
    //! usages
    QString default_ns;
    //! full reflection data version, should be >= api_version
    QString version;
    //! supported api version.
    QString api_version;
    //! Hash of the sourced reflection data.
    QString api_hash;
    struct ImportedData {
        QString module_name;
        QString api_version;
    };
    // Contains imports required to process this ReflectionData.
    QVector<ImportedData> imports;

};

void initContext();
void setConfig(const ModuleConfig &mc);

bool processHeader(const QString &fname, QIODevice *src);

bool exportJson(QIODevice *tgt);
bool exportCpp(QIODevice *tgt);
